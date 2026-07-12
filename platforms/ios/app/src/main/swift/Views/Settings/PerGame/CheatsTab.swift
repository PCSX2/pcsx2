// CheatsTab.swift — Per-game Cheats & Patches category tab.
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct CheatsTab: View {
    @Binding var enabled: Bool
    @Binding var enableGameFixes: Bool
    @Binding var enableGameDBHardwareFixes: Bool
    @Binding var perGameWidescreen: Int
    @Binding var perGameNoInterlace: Int
    @Binding var showCheatsManager: Bool

    let savesToRunningGame: Bool
    let settings: SettingsStore

    var body: some View {
        PerGameTab(title: settings.localized("Cheats & Patches")) {
            Section(settings.localized("Cheats & Patches")) {
                Button {
                    showCheatsManager = true
                } label: {
                    Label(settings.localized("Cheats & Patches"), systemImage: "rectangle.stack.badge.plus")
                }
                Toggle(settings.localized("GameDB Core Fixes"), isOn: $enableGameFixes)
                    .disabled(!enabled)
                Toggle(settings.localized("GameDB Graphics Fixes"), isOn: $enableGameDBHardwareFixes)
                    .disabled(!enabled)
                Picker(settings.localized("Widescreen Patches"), selection: $perGameWidescreen) {
                    Text(settings.localized("Use Global")).tag(-1)
                    Text(settings.localized("Off")).tag(0)
                    Text(settings.localized("On")).tag(1)
                }
                .disabled(!enabled)
                Picker(settings.localized("No-Interlacing Patches"), selection: $perGameNoInterlace) {
                    Text(settings.localized("Use Global")).tag(-1)
                    Text(settings.localized("Off")).tag(0)
                    Text(settings.localized("On")).tag(1)
                }
                .disabled(!enabled)
                Text(settings.localized("If a game looks worse after GameDB, turn off GameDB Graphics Fixes for this game and reset/relaunch it. Core fixes cover timing, clamps, and other compatibility behavior. " + (savesToRunningGame ? "Widescreen and no-interlace patches apply when you save." : "Patch changes apply on next boot.")))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }
}
