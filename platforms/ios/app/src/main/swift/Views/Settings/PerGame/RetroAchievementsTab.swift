// RetroAchievementsTab.swift — Per-game RetroAchievements overrides.
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct RetroAchievementsTab: View {
    @Binding var enabled: Bool
    @Binding var raEnabledOverride: Int
    @Binding var raHardcoreOverride: Int

    let settings: SettingsStore

    var body: some View {
        PerGameTab(title: settings.localized("RetroAchievements")) {
            Section {
                Picker(settings.localized("Enable RetroAchievements"), selection: $raEnabledOverride) {
                    Text(settings.localized("Use Global")).tag(-1)
                    Text(settings.localized("Off")).tag(0)
                    Text(settings.localized("On")).tag(1)
                }
                .disabled(!enabled)

                Picker(settings.localized("Hardcore Mode"), selection: $raHardcoreOverride) {
                    Text(settings.localized("Use Global")).tag(-1)
                    Text(settings.localized("Off")).tag(0)
                    Text(settings.localized("On")).tag(1)
                }
                .disabled(!enabled)
            }

            Section {
                Text(settings.localized("Hardcore mode disables save states, cheats, and slow-motion for this game."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Text(settings.localized("Enable RetroAchievements to On enables achievements for this game even if the global toggle is off."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }
}
