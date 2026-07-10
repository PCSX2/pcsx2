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
    val scroll = remember { ScrollState(0) }
    ControllerAutoScroll(scroll)

    fun apply(updated: Settings) = InGameOverlay.saveSettings(updated)

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .verticalScroll(scroll)
            .verticalScrollbar(scroll),
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
                valueFormatter = { "$it Hz" },
                onChange = { apply(s.copy(framerateNtsc = it.toFloat())) },
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
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                BubbleGridRow {
                    ToggleBubble(str("perf.fix.skipBios"), s.enableFastBoot, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableFastBoot = it))
                    }
                    ToggleBubble(str("perf.fix.gamedbFixes"), s.enableGameFixes, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = it))
                    }
                    ToggleBubble(str("perf.fix.skipMpeg"), s.gamefixSkipMpeg, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixSkipMpeg = it))
                    }
                    ToggleBubble(str("perf.fix.fmvSoftware"), s.gamefixSoftwareRendererFmv, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixSoftwareRendererFmv = it))
                    }
                }
                if (s.gamefixSkipMpeg) {
                    HelpText(str("perf.fix.skipMpeg.warning"))
                }
                BubbleGridRow {
                    ToggleBubble(str("perf.fix.eeTiming"), s.gamefixEETiming, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixEETiming = it))
                    }
                    ToggleBubble(str("perf.fix.instantDma"), s.gamefixInstantDma, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixInstantDma = it))
                    }
                    ToggleBubble(str("perf.fix.blitFps"), s.gamefixBlitInternalFps, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixBlitInternalFps = it))
                    }
                    ToggleBubble(str("perf.fix.fpuMultiply"), s.gamefixFpuMul, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixFpuMul = it))
                    }
                }
                BubbleGridRow {
                    ToggleBubble(str("perf.fix.ophFlag"), s.gamefixOphFlag, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixOphFlag = it))
                    }
                    ToggleBubble(str("perf.fix.gifFifo"), s.gamefixGifFifo, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixGifFifo = it))
                    }
                    ToggleBubble(str("perf.fix.dmaBusy"), s.gamefixDmaBusy, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixDmaBusy = it))
                    }
                    ToggleBubble(str("perf.fix.vif1Stall"), s.gamefixVif1Stall, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixVif1Stall = it))
                    }
                }
                BubbleGridRow {
                    ToggleBubble(str("perf.fix.iBit"), s.gamefixIbit, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixIbit = it))
                    }
                    ToggleBubble(str("perf.fix.fullVu0Sync"), s.gamefixFullVu0Sync, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixFullVu0Sync = it))
                    }
                    ToggleBubble(str("perf.fix.vuAddSub"), s.gamefixVuAddSub, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixVuAddSub = it))
                    }
                    ToggleBubble(str("perf.fix.vuOverflow"), s.gamefixVuOverflow, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixVuOverflow = it))
                    }
                }
                BubbleGridRow {
                    ToggleBubble(str("perf.fix.extraXgkick"), s.gamefixXgkick, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixXgkick = it))
                    }
                    ToggleBubble(str("perf.fix.goemonTlb"), s.gamefixGoemonTlb, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixGoemonTlb = it))
                    }
                    ToggleBubble(str("perf.fix.vuSync"), s.gamefixVuSync, modifier = Modifier.weight(1f)) {
                        apply(s.copy(enableGameFixes = true, gamefixVuSync = it))
                    }
                    Spacer(Modifier.weight(1f))
                }
            }
            HelpText(str("perf.gamedbFixes.legend"))
        }
        SettingsDivider()
        CollapsibleSection(str("perf.advancedSpeedhacks.title")) {
            Spacer(Modifier.height(8.dp))
            // On/Off toggles as a 4-wide bubble grid, matching the Playing-Now
            // action grid in InGameOverlay. Two rows of four cells — last cell
            // is a Spacer so the seven toggles keep uniform widths across both
            // rows. Labels are abbreviated to fit the ~74dp cell.
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                BubbleGridRow {
                    ToggleBubble(str("perf.hack.mtvu"), s.mtvu, modifier = Modifier.weight(1f)) {
                        apply(s.copy(mtvu = it))
                    }
                    ToggleBubble(str("perf.hack.instantVu1"), s.vu1Instant, modifier = Modifier.weight(1f)) {
                        apply(s.copy(vu1Instant = it))
                    }
                    ToggleBubble(str("perf.hack.vuFlagHack"), s.vuFlagHack, modifier = Modifier.weight(1f)) {
                        apply(s.copy(vuFlagHack = it))
                    }
                    ToggleBubble(str("perf.hack.fastCdvd"), s.fastCDVD, modifier = Modifier.weight(1f)) {
                        apply(s.copy(fastCDVD = it))
                    }
                }
                BubbleGridRow {
                    ToggleBubble(str("perf.hack.intcStat"), s.intcStat, modifier = Modifier.weight(1f)) {
                        apply(s.copy(intcStat = it))
                    }
                    ToggleBubble(str("perf.hack.waitLoop"), s.waitLoop, modifier = Modifier.weight(1f)) {
                        apply(s.copy(waitLoop = it))
                    }
                    // Frame Limiter lives on the Play tab's quick toggles — no need
                    // to duplicate it here.
                    // ARMSX2 perf-knob: A/B disable for the arm64 VU1 JIT
                    // NEON peephole fusions. Default on; flip off to confirm
                    // whether a per-game regression traces back to our
                    // matrix-transform / cross-product fusion peepholes.
                    // Block recompilation is not invalidated on toggle —
                    // already-compiled blocks keep their fused path; new
                    // blocks pick up the new gate. Restart the game for a
                    // clean A/B (or hit the recompiler tab to flip a CPU off
                    // then on, which forces a cache rebuild).
                    ToggleBubble(str("perf.hack.vuNeonFusions"), s.vuNeonFusions, modifier = Modifier.weight(1f)) {
                        apply(s.copy(vuNeonFusions = it))
                    }
                }
                // Heavy / aggressive perf levers — off by default, may break
                // some games. Effective on next block recompile; for a clean
                // A/B, restart the game (or bounce a CPU recompiler toggle
                // off→on to force a JIT cache rebuild).
                //
                //   Skip VU Stall Sim — drops the vu1_TestPipes_VU1 BL
                //     entirely. Was 19-32% of total CPU on Futurama / GoW2 /
                //     Ape Escape 3. Breaks games that depend on accurate
                //     FMAC / FDIV / EFU / IALU pipeline-stall timing
                //     (glitched models, missing geometry, audio crackle).
                //   Defer VU Writes — VF stores held in the NEON cache,
                //     committed at flush sites. Saves 1 Str per FMAC pair on
                //     transforms. Known to break SH2 graphics (cross-pair
                //     coherence) and similar.
                BubbleGridRow {
                    ToggleBubble(str("perf.hack.skipVuStallSim"), s.vuSkipStallSim, modifier = Modifier.weight(1f)) {
                        apply(s.copy(vuSkipStallSim = it))
                    }
                    ToggleBubble(str("perf.hack.deferVuWrites"), s.vuDeferredWrites, modifier = Modifier.weight(1f)) {
                        apply(s.copy(vuDeferredWrites = it))
                    }
                    ToggleBubble(str("perf.hack.skipDupeFrames"), s.skipDuplicateFrames, modifier = Modifier.weight(1f)) {
                        apply(s.copy(skipDuplicateFrames = it))
                    }
                    Spacer(Modifier.weight(1f))
                }
            }
            HelpText(str("perf.advancedSpeedhacks.legend"))
        }
    }
}
