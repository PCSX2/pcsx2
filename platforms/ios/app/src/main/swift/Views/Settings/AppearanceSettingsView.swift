// AppearanceSettingsView.swift — Library background customization
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct AppearanceSettingsView: View {
    private enum PresentedEditor: String, Identifiable {
        case colours

        var id: String { rawValue }
    }

    @State private var settings = SettingsStore.shared
    @State private var dynamicPreferences = SettingsStore.shared.dynamicAppearancePreferences
    @State private var paletteTarget: ThemePaletteTarget = .shared
    @State private var presentedEditor: PresentedEditor?
    @State private var isShowingBackgroundOnly = false
    @State private var showPrimaryPicker = false
    @State private var showLandscapePicker = false
    @State private var isAppearanceVisible = false
    @State private var ownsExclusiveBackgroundPreview = false
    @Environment(\.menuTabIsActive) private var menuTabIsActive
    @Environment(\.menuBackgroundHost) private var menuBackgroundHost

    var body: some View {
        Form {
            DynamicBackgroundAppearanceSections(
                preferences: $dynamicPreferences,
                isPreviewActive: shouldRenderDynamicPreview,
                showPaletteEditor: { presentedEditor = .colours }
            )

            Section {
                BackgroundAssetRow(
                    title: settings.localized("Primary Background"),
                    asset: settings.backgroundPrimaryAsset,
                    glyph: "rectangle.portrait",
                    caption: settings.localized("Shown in portrait and anywhere no landscape background is set.")
                ) { showPrimaryPicker = true }
                .modifier(BackgroundSourcePicker(isPresented: $showPrimaryPicker, role: .primary, existingAsset: { settings.backgroundPrimaryAsset }) { updatePrimary($0) })

                BackgroundAssetRow(
                    title: settings.localized("Landscape Background"),
                    asset: settings.backgroundLandscapeAsset,
                    glyph: "rectangle",
                    caption: settings.localized("Optional. Used only when the device is held in landscape.")
                ) { showLandscapePicker = true }
                .modifier(BackgroundSourcePicker(isPresented: $showLandscapePicker, role: .landscape, existingAsset: { settings.backgroundLandscapeAsset }) { updateLandscape($0) })
            } header: {
                Text(settings.localized("Background"))
            } footer: {
                Text(settings.localized("Each orientation keeps its own background. Setting one never overwrites the other."))
            }

            Section {
                Picker(selection: $settings.backgroundFitMode) {
                    ForEach(BackgroundFitMode.allCases) { mode in
                        Text(label(for: mode)).tag(mode)
                    }
                } label: {
                    Label(settings.localized("Portrait Fit Mode"), systemImage: "rectangle.portrait")
                }
                .onChange(of: settings.backgroundFitMode) { _, _ in
                    UISelectionFeedbackGenerator().selectionChanged()
                }

                portraitFitModeHint

                Picker(selection: $settings.backgroundLandscapeFitMode) {
                    ForEach(BackgroundFitMode.allCases) { mode in
                        Text(label(for: mode)).tag(mode)
                    }
                } label: {
                    Label(settings.localized("Landscape Fit Mode"), systemImage: "rectangle")
                }
                .onChange(of: settings.backgroundLandscapeFitMode) { _, _ in
                    UISelectionFeedbackGenerator().selectionChanged()
                }

                landscapeFitModeHint
            } header: {
                Text(settings.localized("Fit Mode"))
            } footer: {
                Text(settings.localized("Each orientation uses its own fit mode."))
            }

            Section {
                Toggle(isOn: $settings.backgroundVideoMuted) {
                    Label(settings.localized("Mute Video"), systemImage: settings.backgroundVideoMuted ? "speaker.slash" : "speaker.wave.2")
                }

                VStack(alignment: .leading, spacing: 6) {
                    HStack {
                        Label(settings.localized("Background Dim"), systemImage: "circle.lefthalf.filled")
                        Spacer()
                        Text(String(format: "%d%%", Int(settings.backgroundDim * 100)))
                            .font(.subheadline.monospacedDigit())
                            .foregroundStyle(.secondary)
                    }
                    Slider(value: $settings.backgroundDim, in: 0.0...1.0, step: 0.05) {
                        Text(settings.localized("Background Dim"))
                    }
                    .accessibilityValue(String(format: "%d%%", Int(settings.backgroundDim * 100)))
                }
                .padding(.vertical, 4)
            }

            Section {
                Toggle(isOn: $settings.backgroundEnabledInBIOS) {
                    Label(settings.localized("BIOS"), systemImage: "cpu")
                }
                Toggle(isOn: $settings.backgroundEnabledInHelp) {
                    Label(settings.localized("Help"), systemImage: "questionmark.circle")
                }
                Toggle(isOn: $settings.backgroundEnabledInSettings) {
                    Label(settings.localized("Settings"), systemImage: "gearshape")
                }
            } header: {
                Text(settings.localized("Show Background In"))
            } footer: {
                Text(settings.localized("The background also shows behind the Games library. Each tab can be toggled independently. Dim or mute from the settings above."))
            }
        }
        .navigationTitle(settings.localized("Appearance"))
        .sheet(item: $presentedEditor, onDismiss: paletteEditorDidDismiss) { _ in
            ThemePaletteEditor(
                target: $paletteTarget,
                preferences: $dynamicPreferences,
                isShowingBackgroundOnly: $isShowingBackgroundOnly,
                dynamicBackground: dynamicPreferences.dynamicBackground,
                onSaveAppearance: saveDynamicAppearance
            )
            .presentationDetents([.large])
        }
        .onAppear {
            if !ownsExclusiveBackgroundPreview {
                menuBackgroundHost?.beginExclusivePreview()
                ownsExclusiveBackgroundPreview = true
            }
            isAppearanceVisible = true
            dynamicPreferences = settings.dynamicAppearancePreferences
        }
        .onDisappear {
            isAppearanceVisible = false
            if ownsExclusiveBackgroundPreview {
                menuBackgroundHost?.endExclusivePreview()
                ownsExclusiveBackgroundPreview = false
            }
        }
    }

    private var shouldRenderDynamicPreview: Bool {
        isAppearanceVisible
            && menuTabIsActive
            && presentedEditor == nil
    }

    @ViewBuilder
    private var portraitFitModeHint: some View {
        switch settings.backgroundFitMode {
        case .fill:
            HintRow(icon: "arrow.up.left.and.arrow.down.right", text: settings.localized("Fill — covers the whole screen, cropping edges if needed."))
        case .fit:
            HintRow(icon: "rectangle.compress.vertical", text: settings.localized("Fit — shows the whole image with bars on the empty sides."))
        case .stretch:
            HintRow(icon: "arrow.left.and.right", text: settings.localized("Stretch — fills the screen, distorting to match."))
        }
    }

    @ViewBuilder
    private var landscapeFitModeHint: some View {
        switch settings.backgroundLandscapeFitMode {
        case .fill:
            HintRow(icon: "arrow.up.left.and.arrow.down.right", text: settings.localized("Fill — covers the whole screen, cropping edges if needed."))
        case .fit:
            HintRow(icon: "rectangle.compress.vertical", text: settings.localized("Fit — shows the whole image with bars on the empty sides."))
        case .stretch:
            HintRow(icon: "arrow.left.and.right", text: settings.localized("Stretch — fills the screen, distorting to match."))
        }
    }

    private func label(for mode: BackgroundFitMode) -> String {
        switch mode {
        case .fill: return settings.localized("Fill")
        case .fit: return settings.localized("Fit")
        case .stretch: return settings.localized("Stretch")
        }
    }

    private func saveDynamicAppearance() {
        settings.dynamicAppearancePreferences = dynamicPreferences
    }

    private func paletteEditorDidDismiss() {
        isShowingBackgroundOnly = false
        dynamicPreferences = settings.dynamicAppearancePreferences
    }

    private func updatePrimary(_ asset: BackgroundAsset?) {
        if asset == nil { BackgroundStorage.remove(settings.backgroundPrimaryAsset) }
        settings.backgroundPrimaryAsset = asset
    }

    private func updateLandscape(_ asset: BackgroundAsset?) {
        if asset == nil { BackgroundStorage.remove(settings.backgroundLandscapeAsset) }
        settings.backgroundLandscapeAsset = asset
    }
}

private struct HintRow: View {
    let icon: String
    let text: String

    var body: some View {
        HStack(alignment: .top, spacing: 8) {
            Image(systemName: icon).font(.caption).foregroundStyle(.secondary).frame(width: 16)
            Text(text).font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
        }
        .padding(.vertical, 2)
    }
}
