// DynamicBackgroundPresentation.swift — ARMSX2 dynamic background integration
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit

struct DynamicBackgroundRendererView: View {
    let preferences: DynamicAppearancePreferences
    @State private var isRenderingEnabled = true

    var body: some View {
        Group {
            if isRenderingEnabled {
                DynamicBackgroundContentView(
                    style: preferences.dynamicBackground,
                    theme: DynamicBackgroundTheme(preferences: preferences)
                )
            }
        }
        .onAppear {
            isRenderingEnabled = true
        }
        .onReceive(NotificationCenter.default.publisher(for: UIScene.willDeactivateNotification)) { _ in
            isRenderingEnabled = false
        }
        .onReceive(NotificationCenter.default.publisher(for: UIScene.didActivateNotification)) { _ in
            isRenderingEnabled = true
        }
        .onReceive(NotificationCenter.default.publisher(for: AppState.releaseMenuBackgroundResourcesNotification)) { _ in
            isRenderingEnabled = false
        }
    }
}

struct DynamicBackgroundContentView: View {
    let style: DynamicBackgroundStyle
    let theme: DynamicBackgroundTheme

    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    @ViewBuilder
    var body: some View {
        if reduceMotion {
            StaticDynamicBackgroundView(theme: theme)
        } else {
            style.makeBackground(theme: theme)
                .id(style.id)
        }
    }
}

private struct StaticDynamicBackgroundView: View {
    let theme: DynamicBackgroundTheme

    var body: some View {
        let deep = theme.sharedColor(index: 0, time: 0)
        let middle = theme.sharedColor(index: 1, time: 0)
        let accent = theme.ribbonColor(index: 2, time: 0)
        let gradient = theme.sharedGradientPoints(from: .topLeading, to: .bottomTrailing)

        ZStack {
            PaletteGradientField(
                colors: [
                    theme.paletteBackgroundColor(deep, darkness: 0.96),
                    theme.paletteBackgroundColor(middle, darkness: 0.72),
                    theme.paletteBackgroundColor(accent, darkness: 0.9),
                    theme.paletteBackgroundColor(deep, darkness: 1),
                ],
                startPoint: gradient.start,
                endPoint: gradient.end,
                curvature: theme.sharedPaletteGradientCurvature
            )

            Circle()
                .fill(accent.opacity(0.18))
                .frame(width: 520, height: 520)
                .blur(radius: 130)
                .offset(x: 140, y: -90)
        }
        .ignoresSafeArea()
    }
}

private extension DynamicBackgroundTheme {
    init(preferences: DynamicAppearancePreferences) {
        self.init(
            sharedPalette: preferences.sharedPalette,
            sharedCustomColor: preferences.sharedCustomColor,
            sharedMultiColor: preferences.sharedMultiColor,
            ribbonPalette: preferences.ribbonPalette,
            ribbonCustomColor: preferences.ribbonCustomColor,
            ribbonMultiColor: preferences.ribbonMultiColor,
            particleSettings: preferences.particleSettings
        )
    }
}

struct DynamicBackgroundAppearanceSections: View {
    @State private var settings = SettingsStore.shared
    @Binding var preferences: DynamicAppearancePreferences
    let isPreviewActive: Bool
    let showPaletteEditor: () -> Void
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    var body: some View {
        Section {
            Toggle(isOn: $settings.dynamicBackgroundsEnabled) {
                Label(
                    settings.localized("Use Dynamic Background"),
                    systemImage: settings.dynamicBackgroundsEnabled ? "waveform.path" : "waveform.path.badge.minus"
                )
            }

            Picker(
                settings.localized("Background Style"),
                selection: styleBinding
            ) {
                ForEach(DynamicBackgroundStyle.allCases) { style in
                    Label(settings.localized(style.title), systemImage: style.systemImage)
                        .tag(style)
                }
            }
            .pickerStyle(.menu)
        } header: {
            Text(settings.localized("Dynamic Backgrounds"))
        } footer: {
            Text(settings.localized("Dynamic backgrounds replace imported library media while enabled. Your portrait and landscape backgrounds stay saved."))
        }

        Section {
            dynamicPreview
        } header: {
            Text(settings.localized("Preview"))
        } footer: {
            if reduceMotion {
                Text(settings.localized("Reduce Motion is enabled, so ARMSX2 shows a still palette preview."))
            } else {
                Text(settings.localized("The preview uses the same renderer as the game library."))
            }
        }

        Section {
            Button(action: showPaletteEditor) {
                HStack {
                    Label(settings.localized("Colours & Effects"), systemImage: "paintpalette.fill")
                    Spacer()
                    Text(paletteSummary)
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                    Image(systemName: "chevron.right")
                        .font(.caption.weight(.semibold))
                        .foregroundStyle(.tertiary)
                }
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
        } footer: {
            Text(settings.localized("Personalize colours, ribbons, particles, motion, and each background's advanced controls."))
        }
    }

    private var dynamicPreview: some View {
        Group {
            if isPreviewActive {
                DynamicBackgroundRendererView(preferences: preferences)
            } else {
                Color.clear
            }
        }
            .frame(maxWidth: .infinity)
            .aspectRatio(16 / 9, contentMode: .fit)
            .clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
            .overlay {
                RoundedRectangle(cornerRadius: 14, style: .continuous)
                    .stroke(.white.opacity(0.14), lineWidth: 0.75)
            }
            .overlay(alignment: .bottomLeading) {
                Label(
                    settings.localized(preferences.dynamicBackground.title),
                    systemImage: preferences.dynamicBackground.systemImage
                )
                .font(.caption.weight(.semibold))
                .foregroundStyle(.white)
                .padding(.horizontal, 10)
                .frame(height: 30)
                .background(.black.opacity(0.42), in: Capsule())
                .padding(10)
            }
            .accessibilityElement(children: .ignore)
            .accessibilityLabel(
                settings.localized("Preview: \(preferences.dynamicBackground.title)")
            )
            .listRowInsets(EdgeInsets(top: 10, leading: 10, bottom: 10, trailing: 10))
    }

    private var styleBinding: Binding<DynamicBackgroundStyle> {
        Binding(
            get: { preferences.dynamicBackground },
            set: { style in
                selectStyle(style)
            }
        )
    }

    private var paletteSummary: String {
        return "\(preferences.sharedPalette.title) · \(preferences.ribbonPalette.title)"
    }

    private func selectStyle(_ style: DynamicBackgroundStyle) {
        guard style != preferences.dynamicBackground else { return }

        if style == .playStation3XMBByMart,
           !preferences.hasSelectedPlayStation3XMBByMart {
            preferences.sharedPalette = .blue
            preferences.sharedCustomColor = nil
            preferences.sharedMultiColor.isEnabled = false
            preferences.ribbonPalette = .cyan
            preferences.ribbonCustomColor = nil
            preferences.ribbonMultiColor.isEnabled = false
            if !preferences.isPlayStation3XMBPresetExplicit {
                preferences.particleSettings.playStation3XMB.gradientPreset = .theme
            }
            preferences.hasSelectedPlayStation3XMBByMart = true
        }

        preferences.dynamicBackground = style
        settings.dynamicAppearancePreferences = preferences
        UISelectionFeedbackGenerator().selectionChanged()
    }

}

extension View {
    func glassSurface(
        tint: Color? = nil,
        interactive: Bool = false,
        clear: Bool = false,
        cornerRadius: CGFloat
    ) -> some View {
        modifier(
            DynamicBackgroundGlassSurfaceModifier(
                tint: tint,
                interactive: interactive,
                clear: clear,
                cornerRadius: cornerRadius
            )
        )
    }
}

private struct DynamicBackgroundGlassSurfaceModifier: ViewModifier {
    let tint: Color?
    let interactive: Bool
    let clear: Bool
    let cornerRadius: CGFloat

    @ViewBuilder
    func body(content: Content) -> some View {
        if #available(iOS 26.0, *) {
            if clear {
                content
                    .glassEffect(
                        .clear.tint(tint).interactive(interactive),
                        in: .rect(cornerRadius: cornerRadius)
                    )
            } else {
                content
                    .glassEffect(
                        .regular.tint(tint).interactive(interactive),
                        in: .rect(cornerRadius: cornerRadius)
                    )
            }
        } else {
            content
                .background(
                    .ultraThinMaterial,
                    in: RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
                )
        }
    }
}
