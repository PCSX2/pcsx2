// GeneralTab.swift — Per-game General category tab.
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct GeneralTab: View {
    @Binding var enabled: Bool
    @Binding var hasGameSettingsIdentity: Bool
    @Binding var showResetAllConfirmation: Bool
    @Binding var statusMessage: String?

    let displayName: String
    let hasPendingChanges: Bool
    let savesToRunningGame: Bool
    let game: ISOEntry
    let settings: SettingsStore

    var body: some View {
        PerGameTab(title: settings.localized("General")) {
            identitySection
            overridesSection
            statusSection
        }
    }

    @ViewBuilder
    private var identitySection: some View {
        Section {
            HStack(spacing: 12) {
                Image(systemName: enabled ? "slider.horizontal.3" : "power")
                    .font(.title3)
                    .foregroundStyle(enabled ? Color.accentColor : Color.secondary)
                    .frame(width: 32, height: 32)
                    .background(Color.accentColor.opacity(enabled ? 0.14 : 0), in: Circle())

                VStack(alignment: .leading, spacing: 4) {
                    Text(displayName)
                        .font(.headline)
                        .lineLimit(2)
                    if let serial = game.metadata["serial"], !serial.isEmpty {
                        Text("\(serial)  ·  CRC \(PadLayoutGameIdentity.normalizedCRC(game.metadata["crc"] ?? ""))")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .textSelection(.enabled)
                    }
                    Text(hasPendingChanges
                         ? (savesToRunningGame
                            ? settings.localized("Unsaved changes — tap Save to apply now.")
                            : settings.localized("Unsaved changes — Save to apply on next boot."))
                         : settings.localized("No pending changes."))
                        .font(.caption)
                        .foregroundStyle(hasPendingChanges ? Color.accentColor : Color.secondary)
                }
            }
            .padding(.vertical, 4)
        }
    }

    @ViewBuilder
    private var overridesSection: some View {
        Section {
            Toggle(settings.localized("Use Per-Game Overrides"), isOn: $enabled)
            Text(settings.localized(savesToRunningGame
                ? "Overrides are saved for this game only and apply when you save, while the game runs."
                : "Overrides are saved for this game only and apply on the next boot of this title."))
                .font(.caption)
                .foregroundStyle(.secondary)
            if !hasGameSettingsIdentity {
                Text("Start this game once before saving its settings.")
                    .font(.caption)
                    .foregroundStyle(.orange)
            }
            Button(role: .destructive) {
                showResetAllConfirmation = true
            } label: {
                Label(settings.localized("Reset All Overrides"), systemImage: "arrow.counterclockwise")
            }
            .disabled(!hasGameSettingsIdentity)
        }
    }

    @ViewBuilder
    private var statusSection: some View {
        if let statusMessage {
            Section {
                Text(statusMessage)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }
}
