// FixesTab.swift — Per-game Fixes & Compatibility category tab.
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct FixesTab: View {
    @Binding var enabled: Bool
    @Binding var perGameRenderer: Int
    @Binding var perGameAAT: Int
    @Binding var perGameTextureInsideRt: Int
    @Binding var perGameFixes: [String: Int]

    let savesToRunningGame: Bool
    let settings: SettingsStore

    var body: some View {
        PerGameTab(title: settings.localized("Fixes & Compatibility")) {
            Section {
                Picker(settings.localized("Renderer"), selection: $perGameRenderer) {
                    Text(settings.localized("Use Global")).tag(-1)
                    Text(settings.localized("Metal (Hardware)")).tag(17)
                    Text(settings.localized("Software")).tag(13)
                }
                .disabled(!enabled)
                Text(settings.localized("Software Renderer is much slower but can fix games that break on Metal. It applies the next time this game boots."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Picker(settings.localized("Accurate Alpha Test"), selection: $perGameAAT) {
                    Text(settings.localized("Use Global")).tag(-1)
                    Text(settings.localized("Off")).tag(0)
                    Text(settings.localized("On")).tag(1)
                }
                .disabled(!enabled)
                Text(settings.localized("Improves the accuracy of transparency and alpha-blended edges. Leave Off unless a game shows halos or broken transparency on Metal. " + (savesToRunningGame ? "Applies when you save." : "Applies on next boot.")))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Picker(settings.localized("Texture Inside RT"), selection: $perGameTextureInsideRt) {
                    Text(settings.localized("Use Global")).tag(-1)
                    Text(settings.localized("Off")).tag(0)
                    Text(settings.localized("Inside Targets")).tag(1)
                    Text(settings.localized("Merge Targets")).tag(2)
                }
                .disabled(!enabled)
                Text(settings.localized("Fixes games that render into areas of the framebuffer they later read back as textures (common half-screen or garbled-graphics fixes). " + (savesToRunningGame ? "Applies when you save." : "Applies on next boot.")))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                ForEach(SettingsStore.gameFixOptions) { option in
                    Picker(settings.localized(option.label), selection: Binding(
                        get: { perGameFixes[option.key] ?? -1 },
                        set: { perGameFixes[option.key] = $0 }
                    )) {
                        Text(settings.localized("Use Global")).tag(-1)
                        Text(settings.localized("Off")).tag(0)
                        Text(settings.localized("On")).tag(1)
                    }
                    .disabled(!enabled)
                    if option.key == "SkipMPEGHack" {
                        Text(settings.localized("Skip MPEG is a last-resort FMV hack that can break interactive cutscenes. Best set per-game."))
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
            } header: {
                Text(settings.localized("Compatibility Overrides"))
            } footer: {
                Text(settings.localized("Override global settings for this game only. Game fixes apply while per-game GameDB Core Fixes is on. " + (savesToRunningGame ? "Most changes apply when you save; the renderer needs a reset." : "Changes apply on next boot.")))
            }
        }
    }
}
