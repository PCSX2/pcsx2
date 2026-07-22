// FramePacingTab.swift — per-game Frame Pacing overrides (mirrors the global panel with Use-Global tags)
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct FramePacingTab: View {
    @Binding var enabled: Bool
    let settings: SettingsStore

    @Binding var perGameFramePacingPreset: Int

    // Per-game individual-control bindings shared with GraphicsTab / AudioTab.
    @Binding var perGameFrameLimiter: Int
    @Binding var perGameTargetFPS: Int
    @Binding var perGameVsyncQueue: Int
    @Binding var perGameSyncToHostRefresh: Int
    @Binding var perGameBufferMS: Int
    @Binding var perGameOutputLatencyMS: Int

    // Triggers the parent's reset confirmation dialog.
    @Binding var showResetConfirmation: Bool

    var body: some View {
        PerGameTab(title: settings.localized("Frame Pacing")) {
            if showsCustomDirtyCaption {
                Text(settings.localized("You've changed individual settings. Pick a preset to return to its values."))
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }

            Section {
                Picker(settings.localized("Preset"), selection: $perGameFramePacingPreset) {
                    Text(settings.localized("Use Global")).tag(-1)
                    ForEach(FramePacingPreset.allCases) { preset in
                        Text(settings.localized(preset.label)).tag(preset.rawValue)
                    }
                }
                .disabled(!enabled)

                Text(String(format: settings.localized("Global preset: %@"), settings.localized(settings.framePacingPreset.label)))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } header: {
                Text(settings.localized("Preset"))
            }

            Section {
                Picker(settings.localized("Frame Limiter"), selection: $perGameFrameLimiter) {
                    Text(settings.localized("Use Global")).tag(-1)
                    Text(settings.localized("On")).tag(1)
                    Text(settings.localized("Off")).tag(0)
                }
                .disabled(!enabled)

                Picker(settings.localized("FPS Target"), selection: $perGameTargetFPS) {
                    Text(settings.localized("Use Global")).tag(-1)
                    ForEach([30, 45, 60, 90, 120], id: \.self) { Text("\($0)").tag($0) }
                }
                .disabled(perGameFrameLimiter == 0 || !enabled)

                Picker(settings.localized("VSync Queue Size"), selection: $perGameVsyncQueue) {
                    Text(settings.localized("Use Global")).tag(-1)
                    ForEach([2, 3, 4, 5, 6, 8, 10, 12, 16], id: \.self) { Text("\($0)").tag($0) }
                }
                .disabled(!enabled)

                Picker(settings.localized("Sync to Host Refresh"), selection: $perGameSyncToHostRefresh) {
                    Text(settings.localized("Use Global")).tag(-1)
                    Text(settings.localized("Off")).tag(0)
                    Text(settings.localized("On")).tag(1)
                }
                .disabled(!enabled)
                Text(settings.localized("Sync to Host Refresh needs a restart to take effect."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Picker(settings.localized("Buffer Size"), selection: $perGameBufferMS) {
                    Text(settings.localized("Use Global")).tag(-1)
                    ForEach([10, 25, 50, 75, 100, 150, 200], id: \.self) { Text("\($0) ms").tag($0) }
                }
                .disabled(!enabled)

                Picker(settings.localized("Output Latency"), selection: $perGameOutputLatencyMS) {
                    Text(settings.localized("Use Global")).tag(-1)
                    ForEach([5, 10, 20, 30, 50, 100, 200], id: \.self) { Text("\($0) ms").tag($0) }
                }
                .disabled(!enabled)
            } header: {
                Text(settings.localized("Individual Settings"))
            }

            Section {
                Button(role: .destructive) {
                    showResetConfirmation = true
                } label: {
                    Text(settings.localized("Reset Per-Game Frame Pacing"))
                }
                .disabled(!enabled)
            }
        }
    }

    // Show the "changed individual settings" caption when this game has
    // overridden any individual control while still on Use Global, or when the
    // preset itself is Custom.
    private var showsCustomDirtyCaption: Bool {
        if perGameFramePacingPreset == FramePacingPreset.custom.rawValue { return true }
        if perGameFramePacingPreset != -1 { return false }
        return perGameFrameLimiter != -1
            || perGameTargetFPS != -1
            || perGameVsyncQueue != -1
            || perGameSyncToHostRefresh != -1
            || perGameBufferMS != -1
            || perGameOutputLatencyMS != -1
    }
}
