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

    let settings: SettingsStore
    let eeCycleRateUseGlobalSentinel: Int
    let fastBootUseGlobalSentinel: Int
    let fastBootOff: Int
    let fastBootOn: Int

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

                Text("Can improve performance in heavy games, but may cause timing or compatibility issues. Reset or relaunch the game after changing it.")
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

                Text("Skips EE cycles to boost performance; higher values are more aggressive and can cause audio or timing issues. Reset or relaunch the game after changing it.")
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
        }
    }

    private static func clampedEECycleRate(_ value: Int) -> Int {
        min(max(value, -3), 3)
    }

    private static func formatEECycleRate(_ value: Int) -> String {
        let clamped = clampedEECycleRate(value)
        return clamped > 0 ? "+\(clamped)" : "\(clamped)"
    }
}
