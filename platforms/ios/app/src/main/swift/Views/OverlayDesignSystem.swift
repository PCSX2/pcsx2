// OverlayDesignSystem.swift — Shared in-game overlay design tokens + components.
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

// MARK: - Overlay Theme

/// In-game overlay design tokens. Scoped to overlays presented over gameplay (the pause menu and
/// Per-Game Settings) — this is NOT a global app theme.
///
/// The surfaces are a graphite/charcoal ladder (shell -> card -> elevated) so an overlay reads as a
/// premium "console command deck" with clear grouping and depth, without going darker overall and
/// without a pure-black-hole background. Chrome uses a controlled glass stack (SwiftUI Material
/// under a graphite tint that wins the hue) so it reads as frosted glass without picking up muddy
/// game colors; the gameplay scrim stays a plain tinted color and never uses Material. Blue is the
/// accent only (never row titles); red is destructive only.
enum OverlayTheme {
    // MARK: Surfaces — opaque graphite ladder (darkest -> lightest)

    /// Panel/shell root surface (darkest). Replaces `Color(.systemBackground)` on overlays.
    static let shell = Color(red: 0.110, green: 0.122, blue: 0.149)          // #1C1F26
    /// Section/card surface — one step lighter than the shell for grouping depth.
    static let card = Color(red: 0.149, green: 0.165, blue: 0.200)           // #262A33
    /// Emphasized / selected / pressed card surface — lightest graphite step.
    static let cardElevated = Color(red: 0.188, green: 0.208, blue: 0.247)   // #30353F
    /// Hairline separator / divider stroke between rows and above a footer.
    static let separator = Color(red: 0.227, green: 0.247, blue: 0.290)      // #3A3F4A
    /// Top stop of the shell gradient — one graphite step lighter than the shell so a panel reads
    /// as lit-from-above frosted glass even when the live blur has nothing game-derived to sample.
    static let shellGradientTop = Color(red: 0.149, green: 0.165, blue: 0.200) // #262A33

    // MARK: Gameplay scrim — lighter, controlled, decoupled from panel opacity

    /// Tinted near-black base for the gameplay dim (NOT pure `Color.black`). Apply at a controlled
    /// opacity (see the scrim opacities below) so the dim only signals "paused" while the opaque
    /// panel carries the premium feel. Phase C wires this into the overlay scrim.
    static let scrimBase = Color(red: 0.039, green: 0.047, blue: 0.063)      // #0A0C10
    /// Lighter gameplay scrim opacities (Phase C replaces the current 0.46 / 0.54 / 0.52).
    static let scrimPad = 0.30
    static let scrimPhoneLandscape = 0.34
    static let scrimPhonePortrait = 0.32
    /// Reduce-Transparency fallbacks (heavier, so RT users still get a strong dim).
    static let scrimPadReduceTransparency = 0.55
    static let scrimPhoneLandscapeReduceTransparency = 0.58
    static let scrimPhonePortraitReduceTransparency = 0.56

    // MARK: Accents — used sparingly

    /// Accent ONLY: header glyph, primary footer button, active/pending indicator. Never row titles.
    static let accent = Color(red: 0.231, green: 0.510, blue: 0.965)         // #3B82F6
    /// Destructive ONLY: Reset ROM / Reset All Overrides.
    static let destructive = Color(red: 0.898, green: 0.282, blue: 0.302)    // #E5484D
    /// Status / warning ONLY (e.g. "Start this game once before saving").
    static let warm = Color(red: 0.941, green: 0.627, blue: 0.125)           // #F0A020

    // MARK: Text

    /// Primary labels, headers, primary button title.
    static let textPrimary = Color(red: 0.925, green: 0.933, blue: 0.949)   // #ECEEF2
    /// Trailing values, captions, subtitles.
    static let textSecondary = Color(red: 0.66, green: 0.69, blue: 0.74)  // #A8B0BD

    // MARK: Footer

    /// Footer surface — matches the shell so a pinned Save/Cancel/Resume bar blends seamlessly.
    static let footer = shell

    // MARK: Glass — controlled frosted chrome (panel shell + cards; never the gameplay scrim)

    static let shellGlassTint: Double = 0.72
    static let cardGlassTint: Double = 0.78
    static let glassTopHighlight = Color.white.opacity(0.10)
    static let cardTopHighlight = Color.white.opacity(0.07)
    static let cardShadow = Color.black.opacity(0.18)
}

// MARK: - Overlay Components

/// Controlled frosted graphite panel surface: a subtle blur layered under a mostly-opaque graphite
/// tint, so the panel reads as premium frosted glass that holds a stable graphite hue instead of
/// picking up muddy game colors. Falls back to a fully opaque shell under Reduce Transparency.
struct OverlayFrostBackground: View {
    @Environment(\.accessibilityReduceTransparency) private var reduceTransparency
    @Environment(\.colorSchemeContrast) private var colorSchemeContrast

    var body: some View {
        let gradient = LinearGradient(
            colors: [OverlayTheme.shellGradientTop, OverlayTheme.shell],
            startPoint: .top,
            endPoint: .bottom
        )
        if reduceTransparency || colorSchemeContrast == .increased {
            gradient
        } else {
            ZStack {
                Rectangle().fill(.regularMaterial)
                gradient.opacity(OverlayTheme.shellGlassTint)
            }
            .overlay(alignment: .top) {
                OverlayTheme.glassTopHighlight
                    .frame(height: 1)
                    .padding(.horizontal, 1)
                    .padding(.top, 0.5)
            }
        }
    }
}

private struct OverlayCompactKey: EnvironmentKey {
    static let defaultValue: Bool = false
}

extension EnvironmentValues {
    var overlayCompact: Bool {
        get { self[OverlayCompactKey.self] }
        set { self[OverlayCompactKey.self] = newValue }
    }
}

/// Opaque overlay shell content. Host this INSIDE `GameOverlayContainer` (which supplies the scrim,
/// bounded card, corner clipping and shadow); the scaffold is the panel's CONTENT, not the card
/// frame. It applies the graphite shell background, clamps Dynamic Type so oversized type cannot
/// break the bounded card, and scopes the accent tint LOCALLY so children inherit the overlay
/// accent without touching the global app tint. Header/footer are composed by the caller.
struct OverlayPanelScaffold<Content: View>: View {
    private let content: Content

    init(@ViewBuilder content: () -> Content) {
        self.content = content()
    }

    var body: some View {
        content
            .dynamicTypeSize(...DynamicTypeSize.accessibility3)
            .tint(OverlayTheme.accent)
            .background(OverlayFrostBackground())
    }
}

/// A lighter-graphite grouping card with an optional small caption header. Gives a consistent depth
/// step on the shell and replaces the ad-hoc `secondarySystemBackground` menu-card pattern.
struct OverlaySectionCard<Content: View>: View {
    @Environment(\.overlayCompact) private var compact
    @Environment(\.accessibilityReduceTransparency) private var reduceTransparency
    @Environment(\.colorSchemeContrast) private var colorSchemeContrast
    private let title: String?
    private let content: Content

    init(title: String? = nil, @ViewBuilder content: () -> Content) {
        self.title = title
        self.content = content()
    }

    private var accessible: Bool {
        reduceTransparency || colorSchemeContrast == .increased
    }

    var body: some View {
        VStack(alignment: .leading, spacing: compact ? 6 : 10) {
            if let title {
                Text(title)
                    .font(compact ? .caption2 : .caption)
                    .foregroundStyle(OverlayTheme.textSecondary)
                    .textCase(.uppercase)
                    .padding(.horizontal, 4)
            }
            // Wrap the rows so they are spaced (a bare @ViewBuilder tuple would pack tight).
            VStack(alignment: .leading, spacing: compact ? 5 : 8) {
                content
            }
        }
        .padding(compact ? 8 : 12)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            OverlayTheme.card.opacity(accessible ? 1.0 : OverlayTheme.cardGlassTint),
            in: RoundedRectangle(cornerRadius: 14, style: .continuous)
        )
        .overlay {
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .stroke(OverlayTheme.cardTopHighlight, lineWidth: 1)
                .opacity(accessible ? 0 : 1)
        }
        .shadow(color: OverlayTheme.cardShadow.opacity(accessible ? 0 : 1), radius: 4, x: 0, y: 2)
    }
}

/// Pinned overlay footer: a full-width primary action (Resume / Save) as a `borderedProminent`
/// button tinted with the overlay accent, plus an optional secondary action, on the shell surface
/// with a top hairline. Apply via `.safeAreaInset(edge: .bottom)`. Pass `compact: true` for the
/// iPad / iPhone-landscape sizing; the default `.large` is the liked iPhone-portrait size.
struct OverlayFooter: View {
    private let primaryLabel: String
    private let primarySystemImage: String
    private let primaryAction: () -> Void
    private let secondaryLabel: String?
    private let secondaryAction: (() -> Void)?
    private let compact: Bool

    init(
        primaryLabel: String,
        primarySystemImage: String,
        primaryAction: @escaping () -> Void,
        secondaryLabel: String? = nil,
        secondaryAction: (() -> Void)? = nil,
        compact: Bool = false
    ) {
        self.primaryLabel = primaryLabel
        self.primarySystemImage = primarySystemImage
        self.primaryAction = primaryAction
        self.secondaryLabel = secondaryLabel
        self.secondaryAction = secondaryAction
        self.compact = compact
    }

    var body: some View {
        VStack(spacing: 0) {
            OverlayTheme.separator
                .frame(height: 0.5)
            VStack(spacing: 8) {
                Button(action: primaryAction) {
                    Label(primaryLabel, systemImage: primarySystemImage)
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .controlSize(compact ? .regular : .large)
                if let secondaryLabel, let secondaryAction {
                    Button(secondaryLabel, action: secondaryAction)
                        .buttonStyle(.bordered)
                        .controlSize(.regular)
                }
            }
            .padding(.horizontal, compact ? 18 : 20)
            .padding(.top, 8)
            .padding(.bottom, compact ? 10 : 14)
        }
        .background(OverlayFrostBackground())
        .tint(OverlayTheme.accent)
    }
}

// MARK: - Header

/// Command-deck header for an overlay: an accent glyph, a primary title, and an optional subtitle
/// (e.g. the game title, middle-truncated). Blue is used only for the glyph; the title stays
/// `textPrimary` so blue never becomes a row/title color.
struct OverlayHeader: View {
    private let systemImage: String
    private let title: String
    private let subtitle: String?
    private let compact: Bool

    init(systemImage: String, title: String, subtitle: String? = nil, compact: Bool) {
        self.systemImage = systemImage
        self.title = title
        self.subtitle = subtitle
        self.compact = compact
    }

    var body: some View {
        HStack(spacing: compact ? 10 : 12) {
            Image(systemName: systemImage)
                .font(.system(size: compact ? 24 : 30))
                .foregroundStyle(OverlayTheme.accent)
                .accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 1) {
                Text(title)
                    .font(compact ? .headline : .title3)
                    .fontWeight(.semibold)
                    .foregroundStyle(OverlayTheme.textPrimary)
                    .lineLimit(1)
                if let subtitle, !subtitle.isEmpty {
                    Text(subtitle)
                        .font(.subheadline)
                        .foregroundStyle(OverlayTheme.textSecondary)
                        .lineLimit(1)
                        .truncationMode(.middle)
                }
            }
            Spacer()
        }
        .padding(.horizontal, compact ? 18 : 20)
        .padding(.top, compact ? 12 : 18)
        .padding(.bottom, compact ? 6 : 10)
        .accessibilityElement(children: .combine)
    }
}

// MARK: - Rows

/// A fixed-min-height overlay action row that GUARANTEES the main label wins over a trailing
/// value: the label gets `layoutPriority(1)` and the trailing value `layoutPriority(-1)` with tail
/// truncation, so on a narrow width the trailing value elides first while the label stays intact.
/// The label is never blue (`textPrimary`); red is used only when `isDestructive`.
struct OverlayActionRow: View {
    @Environment(\.overlayCompact) private var compact
    private let label: String
    private let systemImage: String?
    private let trailingValue: String?
    private let isDestructive: Bool
    private let action: () -> Void

    init(
        label: String,
        systemImage: String? = nil,
        trailingValue: String? = nil,
        isDestructive: Bool = false,
        action: @escaping () -> Void
    ) {
        self.label = label
        self.systemImage = systemImage
        self.trailingValue = trailingValue
        self.isDestructive = isDestructive
        self.action = action
    }

    var body: some View {
        Button(action: action) {
            HStack(spacing: 12) {
                if let systemImage {
                    Image(systemName: systemImage)
                        .frame(width: 22)
                        .foregroundStyle(isDestructive ? OverlayTheme.destructive : OverlayTheme.textSecondary)
                }
                Text(label)
                    .foregroundStyle(isDestructive ? OverlayTheme.destructive : OverlayTheme.textPrimary)
                    .lineLimit(1)
                    .layoutPriority(1)
                Spacer(minLength: 0)
                if let trailingValue {
                    Text(trailingValue)
                        .foregroundStyle(OverlayTheme.textSecondary)
                        .lineLimit(1)
                        .truncationMode(.tail)
                        .layoutPriority(-1)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            .frame(minHeight: compact ? 38 : 44)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}

/// A toggle row on the graphite card: graphite label + accent switch (accent comes from the
/// scaffold's local `.tint`). Matches `OverlayActionRow` height so toggles and actions align.
struct OverlayToggleRow: View {
    @Environment(\.overlayCompact) private var compact
    private let label: String
    private let systemImage: String
    @Binding private var isOn: Bool

    init(label: String, systemImage: String, isOn: Binding<Bool>) {
        self.label = label
        self.systemImage = systemImage
        self._isOn = isOn
    }

    var body: some View {
        Toggle(isOn: $isOn) {
            HStack(spacing: 12) {
                Image(systemName: systemImage)
                    .frame(width: 22)
                    .foregroundStyle(OverlayTheme.textSecondary)
                Text(label)
                    .foregroundStyle(OverlayTheme.textPrimary)
                    .lineLimit(1)
                    .layoutPriority(1)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .frame(minHeight: compact ? 38 : 44)
    }
}
