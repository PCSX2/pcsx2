// EmulatorSettingsView.swift — EE/IOP/VU/boot/speedhack settings
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct EmulatorSettingsView: View {
    @State private var settings = SettingsStore.shared
    @State private var stikDebugOpenFailed = false
    @State private var stikDebugOpenInProgress = false

    var body: some View {
        Form {
            Section {
                Toggle(isOn: Binding(
                    get: { settings.eeCoreType != 1 },
                    set: { settings.eeCoreType = $0 ? 2 : 1 }
                )) {
                    HStack {
                        Text(settings.localized("EE Core"))
                        Spacer()
                        Text(settings.localized(settings.eeCoreType != 1 ? "ARM64 JIT" : "Interpreter"))
                            .foregroundStyle(.secondary)
                            .font(.callout)
                    }
                }
                Toggle(isOn: $settings.iopRecompiler) {
                    HStack {
                        Text("IOP")
                        Spacer()
                        Text(settings.localized(settings.iopRecompiler ? "JIT" : "Interpreter"))
                            .foregroundStyle(.secondary)
                            .font(.callout)
                    }
                }
                Toggle(isOn: $settings.vu0Recompiler) {
                    HStack {
                        Text("VU0")
                        Spacer()
                        Text(settings.localized(settings.vu0Recompiler ? "JIT" : "Interpreter"))
                            .foregroundStyle(.secondary)
                            .font(.callout)
                    }
                }
                Toggle(isOn: $settings.vu1Recompiler) {
                    HStack {
                        Text("VU1")
                        Spacer()
                        Text(settings.localized(settings.vu1Recompiler ? "JIT" : "Interpreter"))
                            .foregroundStyle(.secondary)
                            .font(.callout)
                    }
                }
                Text(settings.localized("Changes take effect on next VM boot."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } header: {
                Text(settings.localized("CPU Recompiler"))
            }

            Section {
                modePicker("EE FPU Round Mode", selection: $settings.eeFpuRoundMode, labels: SettingsStore.roundModeLabels)
                modePicker("VU0 Round Mode", selection: $settings.vu0RoundMode, labels: SettingsStore.roundModeLabels)
                modePicker("VU1 Round Mode", selection: $settings.vu1RoundMode, labels: SettingsStore.roundModeLabels)
                modePicker("EE Clamp Mode", selection: $settings.eeClampMode, labels: SettingsStore.eeClampModeLabels)
                modePicker("VU Clamp Mode", selection: $settings.vuClampMode, labels: SettingsStore.vuClampModeLabels)
            } header: {
                Text(settings.localized("Advanced CPU"))
            } footer: {
                Text(settings.localized("Rounding and clamping can improve compatibility for specific games, but may break others. Changes take effect on the next game boot."))
            }

            Section(settings.localized("StikDebug")) {
                Toggle(settings.localized("Auto-open StikDebug"), isOn: $settings.autoOpenStikDebug)

                Picker(settings.localized("JIT Script"), selection: $settings.jitScriptProtocol) {
                    ForEach(JITScriptProtocol.allCases) { scriptProtocol in
                        Text(settings.localized(scriptProtocol.label)).tag(scriptProtocol)
                    }
                }
                .pickerStyle(.segmented)

                Text(settings.localized(settings.jitScriptProtocol.subtitle))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Button {
                    stikDebugOpenInProgress = true
                    stikDebugOpenFailed = false
                    StikDebugLauncher.open(reason: "emulator-settings") { success in
                        stikDebugOpenInProgress = false
                        stikDebugOpenFailed = !success
                    }
                } label: {
                    Label(settings.localized("Open StikDebug"), systemImage: "bolt.horizontal.circle")
                }
                .disabled(stikDebugOpenInProgress)

                Text(settings.localized("Select the same script here that you run in StikDebug. This only changes the debugger breakpoint protocol used to prepare JIT memory. Fully close and relaunch ARMSX2 after switching scripts."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                if stikDebugOpenFailed {
                    Text(settings.localized("Open StikDebug manually, then run the selected script and relaunch ARMSX2."))
                        .font(.caption)
                        .foregroundStyle(.orange)
                }
            }

            Section(settings.localized("Boot")) {
                Toggle(settings.localized("Fast Boot"), isOn: $settings.fastBoot)
                Text(settings.localized("Skips BIOS intro. Some games require this OFF."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Section(settings.localized("Host Filesystem")) {
                Toggle(settings.localized("Enable Host Filesystem"), isOn: $settings.hostFilesystem)
                Text(settings.localized("Allows PS2 homebrew and ELF tools to access files through the host: device. This is separate from USB/SSD game storage, is off by default, and takes effect on next VM boot."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Section(settings.localized("Memory")) {
                Toggle(settings.localized("Fastmem"), isOn: $settings.fastmem)
                Text(settings.localized("Direct memory mapping for EE. Disable if 3D graphics are broken. Requires restart."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Section(settings.localized("Performance")) {
                Toggle(settings.localized("Frame Limiter"), isOn: $settings.frameLimiterEnabled)

                if settings.frameLimiterEnabled {
                    VStack(alignment: .leading, spacing: 8) {
                        HStack {
                            Text(settings.localized("FPS Target"))
                            Spacer()
                            Text(Self.formatFPS(settings.targetFPS))
                                .foregroundStyle(.secondary)
                                .font(.callout.monospacedDigit())
                        }

                        Slider(
                            value: $settings.targetFPS,
                            in: SettingsStore.minTargetFPS...SettingsStore.maxTargetFPS,
                            step: 1.0
                        )

                        HStack {
                            Text(Self.formatFPS(SettingsStore.minTargetFPS))
                            Spacer()
                            Button(settings.localized("60 FPS")) {
                                settings.targetFPS = SettingsStore.defaultTargetFPS
                            }
                            .buttonStyle(.borderless)
                            Spacer()
                            Text(Self.formatFPS(SettingsStore.maxTargetFPS))
                        }
                        .font(.caption.monospacedDigit())
                        .foregroundStyle(.secondary)
                    }
                } else {
                    HStack {
                        Text(settings.localized("Speed Target"))
                        Spacer()
                        Text(settings.localized("Unlocked"))
                            .foregroundStyle(.orange)
                            .font(.callout.monospacedDigit())
                    }
                }

                HStack {
                    Text(settings.localized("NTSC Base Rate"))
                    Spacer()
                    Text(Self.formatFPS(settings.ntscFramerate))
                        .foregroundStyle(.secondary)
                        .font(.callout.monospacedDigit())
                }

                HStack {
                    Text(settings.localized("PAL Base Rate"))
                    Spacer()
                    Text(Self.formatFPS(settings.palFramerate))
                        .foregroundStyle(.secondary)
                        .font(.callout.monospacedDigit())
                }

                Text(settings.localized("FPS Target maps to PCSX2 Normal Speed: 60 FPS is normal NTSC timing, 30 FPS is about 50% speed, and higher values fast-forward. Turning the limiter OFF unlocks speed and can increase heat and battery drain."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Section(settings.localized("Advanced Emulation")) {
                Toggle(settings.localized("Emulation-Only Mode"), isOn: $settings.emulationOnlyModeEnabled)
                Text(settings.localized("Automatically unloads the selected menus, controls, and optional services for the current emulation session."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Group {
                    VStack(alignment: .leading, spacing: 8) {
                        HStack {
                            Text(settings.localized("Emulation-Only Mode Timer"))
                            Spacer()
                            Text("\(settings.emulationOnlyModeDelaySeconds)s")
                                .foregroundStyle(.secondary)
                                .font(.callout.monospacedDigit())
                        }
                        Slider(
                            value: emulationOnlyModeDelayBinding,
                            in: Double(SettingsStore.emulationOnlyModeDelayRange.lowerBound)...Double(SettingsStore.emulationOnlyModeDelayRange.upperBound),
                            step: 1
                        ) {
                            Text(settings.localized("Emulation-Only Mode Timer"))
                        } minimumValueLabel: {
                            Text("0s")
                        } maximumValueLabel: {
                            Text("15s")
                        }
                    }

                    Toggle(
                        settings.localized("Disable Cheats, Widescreen and Dynamic Patches"),
                        isOn: $settings.emulationOnlyDisablePatches
                    )
                    Toggle(settings.localized("Disable PINE Server"), isOn: $settings.emulationOnlyDisablePINE)
                    Toggle(settings.localized("Disable RetroAchievements"), isOn: $settings.emulationOnlyDisableRetroAchievements)
                    Toggle(settings.localized("Disable PCSX2 Input Recording"), isOn: $settings.emulationOnlyDisableInputRecording)
                    Toggle(settings.localized("Disable OSD and Performance Overlays"), isOn: $settings.emulationOnlyDisableOSD)
                    Toggle(settings.localized("Disable Frame Pacing"), isOn: $settings.emulationOnlyDisableFramePacing)
                    Toggle(settings.localized("Disable Virtual Control Layout"), isOn: $settings.emulationOnlyDisableVirtualControls)
                    Toggle(settings.localized("Disable Quick Menu"), isOn: $settings.emulationOnlyDisableQuickMenu)
                    Toggle(settings.localized("Clear Network Cache"), isOn: $settings.emulationOnlyClearNetworkCache)
                }
                .disabled(!settings.emulationOnlyModeEnabled)

                Text(settings.localized("The timer starts after boot patches and replacement-texture startup complete. Discord Presence is always disabled. All visible cleanup switches default ON, preserving the existing maximum-performance behavior. Disable Frame Pacing stops the optional adaptive frame-time monitor; the core limiter and audio/video timing remain active. Turn a switch off to retain that resource. Without an external controller, the current Virtual Control Layout is retained automatically. Turn Disable Quick Menu off to keep the complete Quick Menu available."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .disabled(!settings.emulationOnlyModeEnabled)
            }

            Section {
                Button(settings.localized("Use VU1 Interpreter Preset")) {
                    settings.applyVU1CompatibilityPreset()
                }
                Button(settings.localized("Use Full Interpreter Preset")) {
                    settings.applyFullInterpreterPreset()
                }
                Text(settings.localized("Use the VU1 preset first for boot crashes or VU1-related texture/rendering glitches. Full Interpreter is much slower, but helps isolate dynarec/JIT issues."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } header: {
                Text(settings.localized("Compatibility"))
            } footer: {
                Text(settings.localized("Changes take effect on next VM boot."))
            }

            Section {
                Toggle(settings.localized("GameDB Automatic Fixes"), isOn: Binding(
                    get: { settings.enableGameFixes && settings.enableGameDBHardwareFixes },
                    set: { enabled in
                        settings.enableGameFixes = enabled
                        settings.enableGameDBHardwareFixes = enabled
                    }
                ))
                Toggle(settings.localized("GameDB Core Fixes"), isOn: $settings.enableGameFixes)
                Toggle(settings.localized("GameDB Graphics Fixes"), isOn: $settings.enableGameDBHardwareFixes)
                Toggle(settings.localized("GameDB PNACH Patches"), isOn: $settings.enablePatches)
                Toggle(settings.localized("Enable PNACH Cheats"), isOn: $settings.enableCheats)
                Toggle(settings.localized("Widescreen Patches"), isOn: $settings.enableWidescreenPatches)
                Toggle(settings.localized("No-Interlacing Patches"), isOn: $settings.enableNoInterlacingPatches)

                Text(settings.localized("GameDB Core Fixes covers timing, clamps, and gamefixes. GameDB Graphics Fixes covers renderer-specific hardware fixes; turn it off globally or per-game if a title looks worse on Metal. Use Cheats & Patches from the in-game quick menu or a game's long-press menu to import and manage patch files."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } header: {
                Text(settings.localized("Patches & Cheats"))
            } footer: {
                Text(settings.localized("Changes take effect on next VM boot."))
            }

            Section {
                ForEach(SettingsStore.gameFixOptions) { option in
                    Toggle(settings.localized(option.label), isOn: Binding(
                        get: { settings.gameFixEnabled(option.key) },
                        set: { settings.setGameFix(option.key, $0) }
                    ))
                    if option.key == "SkipMPEGHack" {
                        Text(settings.localized("Skip MPEG is a last-resort FMV hack that can break interactive cutscenes. Best set per-game."))
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
                .disabled(!settings.enableGameFixes)
            } header: {
                Text(settings.localized("Game Fixes"))
            } footer: {
                Text(settings.localized("Manual game fixes override normal compatibility behavior. Use them only for games that need them. They apply while GameDB Core Fixes is on. Changes take effect on the next game boot."))
            }

            Section {
                Picker(settings.localized("EE Cycle Rate"), selection: $settings.eeCycleRate) {
                    ForEach(-3...3, id: \.self) { value in
                        Text(value > 0 ? "+\(value)" : "\(value)").tag(value)
                    }
                }
                Text(settings.localized("0 = Default. Negative = underclock (stable). Positive = overclock (fast but risky)."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Toggle(settings.localized("Fast CDVD"), isOn: $settings.fastCDVD)
                Toggle(settings.localized("VU1 Instant"), isOn: $settings.vu1Instant)
                Toggle("MTVU", isOn: $settings.mtvu)
                Toggle(settings.localized("Wait Loop Detection"), isOn: $settings.waitLoop)
                Toggle(settings.localized("INTC Stat Hack"), isOn: $settings.intcStat)
                Picker(settings.localized("EE Cycle Skip"), selection: $settings.eeCycleSkip) {
                    ForEach(0...3, id: \.self) { value in
                        Text("\(value)").tag(value)
                    }
                }
                Toggle(settings.localized("VU Flag Hack"), isOn: $settings.vuFlagHack)

                Text(settings.localized("VU1 Instant and MTVU are independent now. MTVU can help some games, but keep it off unless a game specifically benefits on iOS."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Text(settings.localized("Fast CDVD speeds up disc reads and can fix games that stall loading; it rarely causes issues. EE Cycle Skip and VU Flag Hack trade accuracy for speed and can break timing-sensitive games. Wait Loop Detection and INTC Stat Hack are safe, small idle-time savings. Enable speedhacks one at a time per game and relaunch to check."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } header: {
                Text(settings.localized("Speedhacks"))
            } footer: {
                Text(settings.localized("Changes take effect on next VM boot."))
            }

            Section {
                Button(settings.localized("Reset Emulator to Defaults")) {
                    settings.resetEmulatorDefaults()
                }
                .foregroundStyle(.red)
            }
        }
        .navigationTitle(settings.localized("Emulator"))
        .navigationBarTitleDisplayMode(.inline)
        .dynamicTypeSize(...DynamicTypeSize.accessibility3)
    }

    private static func formatFPS(_ value: Float) -> String {
        String(format: "%.2f FPS", value)
    }

    private var emulationOnlyModeDelayBinding: Binding<Double> {
        Binding(
            get: { Double(settings.emulationOnlyModeDelaySeconds) },
            set: { settings.emulationOnlyModeDelaySeconds = Int($0.rounded()) }
        )
    }

    /// Compact labeled picker over a fixed ordered option list (round/clamp modes).
    @ViewBuilder
    private func modePicker(_ title: String, selection: Binding<Int>, labels: [String]) -> some View {
        Picker(settings.localized(title), selection: selection) {
            ForEach(Array(labels.enumerated()), id: \.offset) { index, label in
                Text(settings.localized(label)).tag(index)
            }
        }
    }
}
