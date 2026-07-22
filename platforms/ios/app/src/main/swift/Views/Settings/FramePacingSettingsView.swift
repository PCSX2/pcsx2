// FramePacingSettingsView.swift — consolidated Frame Pacing surface (presets + individual controls)
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct FramePacingSettingsView: View {
    @State private var settings = SettingsStore.shared
    @State private var presetDetailsTarget: FramePacingPreset?
    @State private var showResetConfirmation = false
    @State private var hardcoreActive = ARMSX2Bridge.isRetroAchievementsHardcoreActive()

    var body: some View {
        Form {
            if settings.framePacingPreset == .custom {
                Text(settings.localized("You've changed individual settings. Pick a preset to return to its values."))
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }

            Section {
                ForEach(FramePacingPreset.allCases) { preset in
                    presetRow(preset)
                }
            } header: {
                Text(settings.localized("Preset"))
            }

            Section {
                frameLimiterRows

                Stepper("\(settings.localized("Queue Size")): \(settings.vsyncQueueSize)",
                        value: $settings.vsyncQueueSize,
                        in: 2...16)

                Toggle(settings.localized("Sync to Host Refresh"), isOn: $settings.syncToHostRefresh)
                Text(settings.localized("Sync to Host Refresh needs a restart to take effect."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Stepper("\(settings.localized("Audio Buffer")): \(settings.audioBufferMs) ms",
                        value: $settings.audioBufferMs,
                        in: 10...200)

                Stepper("\(settings.localized("Output Latency")): \(settings.audioOutputLatencyMs) ms",
                        value: $settings.audioOutputLatencyMs,
                        in: 5...200)
            } header: {
                Text(settings.localized("Individual Settings"))
            }

            Section {
                Button(role: .destructive) {
                    showResetConfirmation = true
                } label: {
                    Text(settings.localized("Reset Frame Pacing to Defaults"))
                }
            }
        }
        .navigationTitle(settings.localized("Frame Pacing"))
        .navigationBarTitleDisplayMode(.inline)
        .sheet(item: $presetDetailsTarget) { preset in
            PresetDetailsSheet(preset: preset)
        }
        .confirmationDialog(
            settings.localized("Reset Frame Pacing?"),
            isPresented: $showResetConfirmation,
            titleVisibility: .visible
        ) {
            Button(settings.localized("Reset"), role: .destructive) {
                settings.applyFramePacingPreset(.optimal)
                settings.framePacingPreset = .optimal
            }
            Button(settings.localized("Cancel"), role: .cancel) {}
        } message: {
            Text(settings.localized("This restores the Optimal preset values. Your individual pacing changes are replaced."))
        }
        .onAppear {
            hardcoreActive = ARMSX2Bridge.isRetroAchievementsHardcoreActive()
        }
        .onReceive(NotificationCenter.default.publisher(for: Notification.Name("ARMSX2RetroAchievementsStateChanged"))) { _ in
            hardcoreActive = ARMSX2Bridge.isRetroAchievementsHardcoreActive()
        }
    }

    @ViewBuilder
    private var frameLimiterRows: some View {
        Toggle(settings.localized("Enable Limiter"), isOn: Binding(
            get: { settings.frameLimiterEnabled },
            set: { enabled in
                settings.frameLimiterEnabled = enabled
                enforceHardcoreSpeedFloorIfNeeded()
            }
        ))

        if settings.frameLimiterEnabled {
            VStack(alignment: .leading, spacing: 10) {
                HStack {
                    Text(settings.localized("FPS Target"))
                    Spacer()
                    Text(Self.formatFPS(settings.targetFPS))
                        .foregroundStyle(.secondary)
                        .font(.callout.monospacedDigit())
                }

                Slider(
                    value: Binding(
                        get: { settings.targetFPS },
                        set: { value in
                            settings.targetFPS = value
                            enforceHardcoreSpeedFloorIfNeeded()
                        }
                    ),
                    in: SettingsStore.minTargetFPS...SettingsStore.maxTargetFPS,
                    step: 1.0
                )

                HStack {
                    quickTargetButton(30)
                    quickTargetButton(45)
                    quickTargetButton(60)
                    quickTargetButton(90)
                    quickTargetButton(120)
                }
            }
        } else {
            Text(settings.localized("Limiter is OFF. Games can run above normal speed and may draw more power."))
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }

    @ViewBuilder
    private func presetRow(_ preset: FramePacingPreset) -> some View {
        Button {
            settings.framePacingPreset = preset
        } label: {
            HStack(alignment: .top, spacing: 12) {
                VStack(alignment: .leading, spacing: 4) {
                    Text(settings.localized(preset.label))
                        .font(.body)
                        .foregroundStyle(.primary)
                    Text(settings.localized(preset.caption))
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }

                Spacer(minLength: 8)

                Button {
                    presetDetailsTarget = preset
                } label: {
                    Image(systemName: "info.circle")
                }
                .accessibilityLabel(settings.localized("Preset details"))
                .buttonStyle(.borderless)

                if settings.framePacingPreset == preset {
                    Image(systemName: "checkmark")
                        .foregroundStyle(Color.accentColor)
                        .accessibilityHidden(true)
                }
            }
            .contentShape(Rectangle())
            .frame(minHeight: 44)
        }
        .buttonStyle(.plain)
    }

    private func enforceHardcoreSpeedFloorIfNeeded() {
        guard hardcoreActive else { return }
        let minimumFPS = settings.ntscFramerate
        if settings.frameLimiterEnabled && settings.targetFPS < minimumFPS {
            settings.targetFPS = minimumFPS
        }
    }

    private func quickTargetButton(_ fps: Float) -> some View {
        Button(Self.formatCompactFPS(fps)) {
            settings.frameLimiterEnabled = true
            settings.targetFPS = fps
            enforceHardcoreSpeedFloorIfNeeded()
        }
        .disabled(hardcoreActive && fps < settings.ntscFramerate)
        .buttonStyle(.bordered)
        .font(.caption.monospacedDigit())
    }

    private static func formatFPS(_ value: Float) -> String {
        String(format: "%.0f FPS", value)
    }

    private static func formatCompactFPS(_ value: Float) -> String {
        String(format: "%.0f", value)
    }
}

private struct PresetDetailsSheet: View {
    let preset: FramePacingPreset
    private var settings: SettingsStore { SettingsStore.shared }

    var body: some View {
        NavigationStack {
            ScrollView {
                Text(settings.localized(preset.details))
                    .font(.body)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding()
            }
            .navigationTitle(settings.localized(preset.label))
            .navigationBarTitleDisplayMode(.inline)
        }
        .presentationDetents([.medium])
    }
}

private extension FramePacingPreset {
    var caption: String {
        switch self {
        case .optimal:
            return "Balanced for iOS. Good smoothness with low input lag. Recommended for most games."
        case .smooth:
            return "Largest buffer and most tolerant of jitter. Adds a little input lag."
        case .lowLatency:
            return "Tighter audio and the smallest safe queue. Controls feel snappier; may stutter on heavy games."
        case .batterySaver:
            return "Caps the target FPS to stretch battery. Slower games feel fine; fast games run under speed."
        case .custom:
            return "Your individual pacing settings. Changing any control switches you here."
        }
    }

    var details: String {
        switch self {
        case .optimal:
            return "Tuned for iPhone and iPad. Balances a small vsync queue with safe audio latency so most games feel responsive without stutter. This is the default for fresh installs."
        case .smooth:
            return "Uses a larger vsync queue and audio buffer so the emulator can absorb frame-time spikes without crackle or stutter. Input lag goes up slightly. Good for games that stream or hitch."
        case .lowLatency:
            return "Shrinks the vsync queue and audio latency so button presses land as fast as possible on iOS. Demanding games may stutter; switch back to Optimal if they do."
        case .batterySaver:
            return "Lowers the target FPS so the emulator does less work and the device draws less power. Best for slow-paced or turn-based games on long trips. Fast action games will feel slower than intended."
        case .custom:
            return "You have changed individual pacing settings. They stay exactly as you set them until you pick a preset again."
        }
    }
}
