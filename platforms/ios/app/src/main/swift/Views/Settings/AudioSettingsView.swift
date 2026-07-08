// AudioSettingsView.swift — emulator volume and SPU2 output settings
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct AudioSettingsView: View {
    @State private var settings = SettingsStore.shared

    var body: some View {
        Form {
            Section {
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        Text(settings.localized("Emulator Volume"))
                        Spacer()
                        Text(Self.formatPercent(settings.emulatorVolumePercent))
                            .foregroundStyle(.secondary)
                            .font(.callout.monospacedDigit())
                    }

                    Slider(
                        value: Binding(
                            get: { Double(settings.emulatorVolumePercent) },
                            set: { settings.emulatorVolumePercent = Int($0.rounded()) }
                        ),
                        in: 0...150,
                        step: 1
                    )
                    .accessibilityLabel(settings.localized("Emulator Volume"))
                    .accessibilityValue(Self.formatPercent(settings.emulatorVolumePercent))
                    .accessibilityHint(settings.localized("Adjusts emulator game audio without changing iOS system volume or other apps."))

                    HStack {
                        Text("0%")
                        Spacer()
                        Button(settings.localized("Reset")) {
                            settings.emulatorVolumePercent = SettingsStore.defaultEmulatorVolumePercent
                        }
                        .buttonStyle(.borderless)
                        Spacer()
                        Text("150%")
                    }
                    .font(.caption.monospacedDigit())
                    .foregroundStyle(.secondary)
                }

                Text(settings.localized("Controls emulator and game audio only. iOS system volume and other apps stay separate."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } header: {
                Text(settings.localized("Volume"))
            }

            Section {
                Toggle(settings.localized("Time Stretch"), isOn: $settings.audioTimeStretch)
                Text(settings.localized("Keeps audio in sync by stretching it during speed changes. Turn off if you hear pops or pitch issues."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                intSliderRow("Buffer Size", value: $settings.audioBufferMs, range: 10...200, suffix: " ms", defaultValue: 50)
                intSliderRow("Output Latency", value: $settings.audioOutputLatencyMs, range: 5...200, suffix: " ms", defaultValue: 20)
                intSliderRow("Fast-Forward Volume", value: $settings.audioFastForwardVolume, range: 0...200, suffix: "%", defaultValue: 100)

                Text(settings.localized("Lower buffer or latency reduces lag but can cause crackling. Fast-forward volume is a percentage of normal volume used while fast-forwarding."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } header: {
                Text(settings.localized("Audio Output"))
            } footer: {
                Text(settings.localized("Audio output changes apply without restarting the game."))
            }

            Section {
                Toggle(settings.localized("Left/Right Channel Swap"), isOn: $settings.audioSwapChannels)
                Text(settings.localized("Swaps the left and right channels. Fixes reversed stereo on flipped-speaker or reverse-landscape devices."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            } header: {
                Text(settings.localized("Channels"))
            }
        }
        .navigationTitle(settings.localized("Audio"))
        .navigationBarTitleDisplayMode(.inline)
    }

    private static func formatPercent(_ value: Int) -> String {
        "\(SettingsStore.clampedEmulatorVolumePercent(value))%"
    }

    /// Labeled integer slider with displayed value, range labels, and a reset button.
    @ViewBuilder
    private func intSliderRow(_ title: String, value: Binding<Int>, range: ClosedRange<Int>, suffix: String, defaultValue: Int) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text(settings.localized(title))
                Spacer()
                Text("\(value.wrappedValue)\(suffix)")
                    .foregroundStyle(.secondary)
                    .font(.callout.monospacedDigit())
            }
            Slider(value: Binding(
                get: { Double(value.wrappedValue) },
                set: { value.wrappedValue = Int($0.rounded()) }
            ), in: Double(range.lowerBound)...Double(range.upperBound))
            HStack {
                Text("\(range.lowerBound)\(suffix)")
                Spacer()
                Button(settings.localized("Reset")) { value.wrappedValue = defaultValue }
                    .buttonStyle(.borderless)
                Spacer()
                Text("\(range.upperBound)\(suffix)")
            }
            .font(.caption.monospacedDigit())
            .foregroundStyle(.secondary)
        }
    }
}
