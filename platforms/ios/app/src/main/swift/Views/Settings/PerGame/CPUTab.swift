// CPUTab.swift — Per-game CPU & Speedhacks category tab.
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct CPUTab: View {
    @Binding var enabled: Bool
    @Binding var eeCoreType: Int
    @Binding var mtvu: Bool
    @Binding var eeCycleRate: Int
    @Binding var globalEECycleRate: Int
    @Binding var eeCycleSkip: Int
    @Binding var globalEECycleSkip: Int
    @Binding var fastBoot: Int
    @Binding var globalFastBoot: Bool
    @Binding var perGameIOP: Int
    @Binding var perGameVU0: Int
    @Binding var perGameVU1: Int
    @Binding var perGameEEFpuRound: Int
    @Binding var perGameVU0Round: Int
    @Binding var perGameVU1Round: Int
    @Binding var perGameEEClamp: Int
    @Binding var perGameVUClamp: Int

    let savesToRunningGame: Bool
    let settings: SettingsStore
    let eeCycleRateUseGlobalSentinel: Int
    let fastBootUseGlobalSentinel: Int
    let fastBootOff: Int
    let fastBootOn: Int
    let globalEEFpuRound: Int
    let globalVU0Round: Int
    let globalVU1Round: Int
    let globalEEClamp: Int
    let globalVUClamp: Int

    var body: some View {
        PerGameTab(title: settings.localized("CPU & Speedhacks")) {
            Section(settings.localized("CPU")) {
                Picker(settings.localized("EE Core"), selection: $eeCoreType) {
                    Text(settings.localized("ARM64 JIT")).tag(2)
                    Text(settings.localized("Interpreter")).tag(1)
                }
                .disabled(!enabled)

                Text(settings.localized("Interpreter is slower, but can help isolate EE JIT crashes for specific games. Reset/relaunch after changing it."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Toggle("MTVU", isOn: $mtvu)
                    .disabled(!enabled)
                Text(settings.localized("MTVU can improve performance and may help some visual issues, but can cause compatibility problems. Reset/relaunch after changing it."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Picker(settings.localized("IOP Recompiler"), selection: $perGameIOP) {
                    Text(settings.localized("Use Global")).tag(-1)
                    Text(settings.localized("ARM64 JIT")).tag(1)
                    Text(settings.localized("Interpreter")).tag(0)
                }
                .disabled(!enabled)
                Picker(settings.localized("VU0 Recompiler"), selection: $perGameVU0) {
                    Text(settings.localized("Use Global")).tag(-1)
                    Text(settings.localized("JIT")).tag(1)
                    Text(settings.localized("Interpreter")).tag(0)
                }
                .disabled(!enabled)
                Picker(settings.localized("VU1 Recompiler"), selection: $perGameVU1) {
                    Text(settings.localized("Use Global")).tag(-1)
                    Text(settings.localized("JIT")).tag(1)
                    Text(settings.localized("Interpreter")).tag(0)
                }
                .disabled(!enabled)

                Text(settings.localized("IOP, VU0, and VU1 handle PS2 sub-processors. JIT is much faster; Interpreter is a fallback for the rare game that breaks under JIT. Reset or relaunch the game after changing these."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Section("Performance / Compatibility") {
                Picker("EE Cycle Rate", selection: $eeCycleRate) {
                    Text("Global Default (\(Self.formatEECycleRate(globalEECycleRate)))").tag(eeCycleRateUseGlobalSentinel)
                    ForEach(-3...3, id: \.self) { value in
                        Text(Self.formatEECycleRate(value)).tag(value)
                    }
                }
                .disabled(!enabled)

                Button("Reset EE Cycle Rate to Global") {
                    eeCycleRate = eeCycleRateUseGlobalSentinel
                }
                .disabled(!enabled || eeCycleRate == eeCycleRateUseGlobalSentinel)

                Text(settings.localized("Can improve performance in heavy games, but may cause timing or compatibility issues. " + (savesToRunningGame ? "Takes effect when you save." : "Takes effect on next boot.")))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Picker("EE Cycle Skip", selection: $eeCycleSkip) {
                    Text("Global Default (\(globalEECycleSkip))").tag(-1)
                    ForEach(0...3, id: \.self) { value in
                        Text("\(value)").tag(value)
                    }
                }
                .disabled(!enabled)

                Button("Reset EE Cycle Skip to Global") {
                    eeCycleSkip = -1
                }
                .disabled(!enabled || eeCycleSkip == -1)

                Text(settings.localized("Skips EE cycles to boost performance; higher values are more aggressive and can cause audio or timing issues. " + (savesToRunningGame ? "Takes effect when you save." : "Takes effect on next boot.")))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Picker("Fast Boot", selection: $fastBoot) {
                    Text("Global Default (\(globalFastBoot ? "On" : "Off"))").tag(fastBootUseGlobalSentinel)
                    Text("On").tag(fastBootOn)
                    Text("Off").tag(fastBootOff)
                }
                .disabled(!enabled)

                Button("Reset Fast Boot to Global") {
                    fastBoot = fastBootUseGlobalSentinel
                }
                .disabled(!enabled || fastBoot == fastBootUseGlobalSentinel)

                Text("Some games may need Fast Boot on or off to avoid looping at the disc screen. Reset or relaunch the game after changing it.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Section {
                modeOverridePicker("EE FPU Round Mode", selection: $perGameEEFpuRound,
                                   globalValue: globalEEFpuRound, labels: SettingsStore.roundModeLabels)
                modeOverridePicker("VU0 Round Mode", selection: $perGameVU0Round,
                                   globalValue: globalVU0Round, labels: SettingsStore.roundModeLabels)
                modeOverridePicker("VU1 Round Mode", selection: $perGameVU1Round,
                                   globalValue: globalVU1Round, labels: SettingsStore.roundModeLabels)
                modeOverridePicker("EE Clamp Mode", selection: $perGameEEClamp,
                                   globalValue: globalEEClamp, labels: SettingsStore.eeClampModeLabels)
                modeOverridePicker("VU Clamp Mode", selection: $perGameVUClamp,
                                   globalValue: globalVUClamp, labels: SettingsStore.vuClampModeLabels)

                Text("Rounding and clamping can improve compatibility for specific games, but may break others. Reset or relaunch the game after changing these.")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } header: {
                Text("Advanced CPU")
            }
        }
    }

    /// A per-game round/clamp picker whose "use global" option (-1) reads the inherited
    /// global level from `labels`, matching the "Global Default (…)" style used elsewhere.
    @ViewBuilder
    private func modeOverridePicker(_ title: String, selection: Binding<Int>,
                                    globalValue: Int, labels: [String]) -> some View {
        Picker(title, selection: selection) {
            Text("Global Default (\(labels[min(max(globalValue, 0), labels.count - 1)]))").tag(-1)
            ForEach(Array(labels.enumerated()), id: \.offset) { index, label in
                Text(label).tag(index)
            }
        }
        .disabled(!enabled)
    }

    private static func clampedEECycleRate(_ value: Int) -> Int {
        min(max(value, -3), 3)
    }

    private static func formatEECycleRate(_ value: Int) -> String {
        let clamped = clampedEECycleRate(value)
        return clamped > 0 ? "+\(clamped)" : "\(clamped)"
    }
}
