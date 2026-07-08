// OverlaySettingsView.swift — OSD preset selector
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct OverlaySettingsView: View {
    @State private var settings = SettingsStore.shared

    var body: some View {
        Form {
            Section(settings.localized("Performance Overlay")) {
                Picker(settings.localized("Preset"), selection: $settings.osdPreset) {
                    ForEach(OsdPreset.allCases, id: \.self) { preset in
                        Text(settings.localized(preset.label)).tag(preset)
                    }
                }
                .pickerStyle(.segmented)

                Picker(settings.localized("Position"), selection: $settings.osdPerformancePosition) {
                    Text(settings.localized("Hidden")).tag(0)
                    Text(settings.localized("Top Left")).tag(1)
                    Text(settings.localized("Top Right")).tag(3)
                }
            }

            Section {
                Toggle(settings.localized("On-screen Notifications"), isOn: $settings.osdShowMessages)
                Text(settings.localized("Shows transient in-game messages such as shader compilation, save states, and settings-applied notices. Turn off to hide them. Critical errors and alerts are not affected."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } header: {
                Text(settings.localized("Notifications"))
            }

            Section(settings.localized("Displayed Items")) {
                Toggle(settings.localized("Show FPS"), isOn: $settings.osdShowFPS)
                Toggle(settings.localized("Show VPS"), isOn: $settings.osdShowVPS)
                Toggle(settings.localized("Show Speed"), isOn: $settings.osdShowSpeed)
                Toggle(settings.localized("Show CPU"), isOn: $settings.osdShowCPU)
                Toggle(settings.localized("Show GPU"), isOn: $settings.osdShowGPU)
                Toggle(settings.localized("Show Resolution"), isOn: $settings.osdShowResolution)
                Toggle(settings.localized("Show GS Stats"), isOn: $settings.osdShowGSStats)
                Toggle(settings.localized("Show Indicators"), isOn: $settings.osdShowIndicators)
                Toggle(settings.localized("Show Settings"), isOn: $settings.osdShowSettings)
                Toggle(settings.localized("Show Inputs"), isOn: $settings.osdShowInputs)
                Toggle(settings.localized("Show Frame Times"), isOn: $settings.osdShowFrameTimes)
                Toggle(settings.localized("Show Version"), isOn: $settings.osdShowVersion)
                Toggle(settings.localized("Show Hardware Info"), isOn: $settings.osdShowHardwareInfo)
                Toggle(settings.localized("Show Texture Replacements"), isOn: $settings.osdShowTextureReplacements)
                Toggle(settings.localized("Show Device Stats"), isOn: $settings.osdShowDeviceStats)
            }

            Section(settings.localized("Notes")) {
                Text(settings.localized("Device Stats adds battery, iOS heat state, Low Power Mode, and emulator RAM usage. iOS does not expose exact CPU/GPU temperatures, so Heat is an OS-reported warning level rather than a thermometer reading."))
                    .foregroundStyle(.secondary)
            }
        }
        .navigationTitle(settings.localized("Overlay"))
        .navigationBarTitleDisplayMode(.inline)
    }
}
