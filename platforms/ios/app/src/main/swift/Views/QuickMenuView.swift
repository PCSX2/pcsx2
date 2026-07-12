// QuickMenuView.swift — In-game pause menu card
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

/// Destinations the pause menu hands back to the host to present. (Also the associated payload of
/// the host's overlay route state machine.)
enum QuickMenuDestination: Equatable {
    case perGame, speed, saveStates, cheats, retroAchievements, padLayout, resetROM
}

/// Native in-game pause menu: a premium opaque graphite "command deck" presented by the host as a
/// bounded card over a lighter, controlled dim of the paused gameplay. The host pauses the VM
/// while this card is shown. Toggles are bound directly; everything else is routed back to the
/// host through closures (unchanged from before) so the existing panels, child-screen routing and
/// confirmation flows stay exactly as in Phase A.
///
/// Layout adapts to the card's geometry (read via a GeometryReader, since the host frames this
/// view to the bounded card size): two columns when the card is comfortably wide, one column
/// otherwise. Resume is pinned inside the panel footer so it never detaches or hides rows.
struct QuickMenuView: View {
    let settings: SettingsStore
    @Binding var padVisible: Bool
    @Binding var fullScreen: Bool
    @Binding var menuButtonHidden: Bool

    let vmMenuAvailable: Bool
    let gameMenuAvailable: Bool
    let virtualPadHiddenByController: Bool
    let gameTitle: String?
    let controllerSkinMenu: AnyView
    let discMenu: AnyView
    let variant: PauseLayoutVariant
    let activePadLayoutName: String

    let onCycleOSD: () -> Void
    let onOpen: (QuickMenuDestination) -> Void
    let onClearCache: () -> Void
    let onBackToMenu: () -> Void
    let onResume: () -> Void

    /// Compact sizing for the header/footer on iPad (any orientation) and iPhone landscape; the
    /// larger, liked sizing is reserved for iPhone portrait.
    private var compact: Bool { variant != .phonePortrait }

    var body: some View {
        // The host (GameOverlayContainer) frames this view to the bounded card size; read that
        // size here to decide whether the geometry comfortably supports two columns.
        GeometryReader { geo in
            overlayBody(width: geo.size.width, height: geo.size.height)
        }
    }

    @ViewBuilder
    private func overlayBody(width: CGFloat, height: CGFloat) -> some View {
        if variant == .phoneLandscape {
            landscapeBody(width: width, height: height)
        } else {
            portraitBody(width: width, height: height)
        }
    }

    @ViewBuilder
    private func portraitBody(width: CGFloat, height: CGFloat) -> some View {
        OverlayPanelScaffold {
            VStack(spacing: 0) {
                OverlayHeader(
                    systemImage: "pause.circle.fill",
                    title: settings.localized("Paused"),
                    subtitle: gameTitle,
                    compact: compact
                )
                scrollContent(twoColumns: supportsTwoColumns(width: width, height: height))
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        }
    }

    @ViewBuilder
    private func landscapeBody(width: CGFloat, height: CGFloat) -> some View {
        OverlayPanelScaffold {
            VStack(spacing: 0) {
                LandscapeCommandBar(
                    settings: settings,
                    gameTitle: gameTitle,
                    onResume: onResume,
                    iconOnly: width < 380
                )
                ScrollView {
                    cardsContent(twoColumns: width > 700)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        }
        .environment(\.overlayCompact, true)
    }

    /// Two columns only when the card is wide enough to keep both columns comfortable.
    /// iPad portrait and short/small phone-landscape devices fall back to one column.
    /// iPhone portrait always uses a single-column scroll layout — the screen is
    /// too narrow for two comfortable columns, even on Plus/Max devices. Landscape
    /// and iPad still get two columns when wide enough.
    private func supportsTwoColumns(width: CGFloat, height: CGFloat) -> Bool {
        switch variant {
        case .phonePortrait:
            return false
        case .ipadTwoColumn:
            return width >= 500 && height >= 320
        case .phoneLandscape:
            return width >= 570 && height >= 300
        }
    }

    @ViewBuilder
    private func cardsContent(twoColumns: Bool) -> some View {
        if twoColumns {
            HStack(alignment: .top, spacing: 12) {
                VStack(spacing: variant == .phoneLandscape ? 11 : 14) {
                    OverlaySectionCard(title: settings.localized("Quick Actions")) { quickActionsRows }
                    OverlaySectionCard(title: settings.localized("This Game")) { thisGameRows }
                }
                VStack(spacing: variant == .phoneLandscape ? 11 : 14) {
                    OverlaySectionCard(title: settings.localized("Game Tools")) { gameToolsRows }
                    OverlaySectionCard(title: settings.localized("Reset & Exit")) { resetAndExitRows }
                }
            }
            .padding(.horizontal, 16)
            .padding(.top, variant == .phoneLandscape ? 8 : 10)
            .padding(.bottom, variant == .phoneLandscape ? 8 : 10)
        } else {
            VStack(spacing: 14) {
                OverlaySectionCard(title: settings.localized("Quick Actions")) { quickActionsRows }
                OverlaySectionCard(title: settings.localized("This Game")) { thisGameRows }
                OverlaySectionCard(title: settings.localized("Game Tools")) { gameToolsRows }
                OverlaySectionCard(title: settings.localized("Reset & Exit")) { resetAndExitRows }
            }
            .padding(.horizontal, 16)
            .padding(.top, variant == .phoneLandscape ? 8 : 10)
            .padding(.bottom, variant == .phoneLandscape ? 8 : 10)
        }
    }

    @ViewBuilder
    private func scrollContent(twoColumns: Bool) -> some View {
        ScrollView {
            cardsContent(twoColumns: twoColumns)
        }
        .safeAreaInset(edge: .bottom, spacing: 0) {
            OverlayFooter(
                primaryLabel: settings.localized("Resume"),
                primarySystemImage: "play.fill",
                primaryAction: onResume,
                compact: compact
            )
        }
    }

    // MARK: - Section row content

    @ViewBuilder private var quickActionsRows: some View {
        OverlayActionRow(
            label: settings.localized("OSD"),
            systemImage: "speedometer",
            trailingValue: settings.localized(settings.osdPreset.label),
            action: onCycleOSD
        )
        .accessibilityHint(settings.localized("Cycles the on-screen display"))

        OverlayToggleRow(label: settings.localized("Virtual Pad"), systemImage: "gamecontroller", isOn: $padVisible)
            .accessibilityHint(settings.localized("Show or hide the on-screen controls"))

        if virtualPadHiddenByController {
            Text(settings.localized("Hidden while controller is connected"))
                .font(.caption)
                .foregroundStyle(OverlayTheme.textSecondary)
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.leading, 34)
                .padding(.bottom, 4)
        }

        OverlayToggleRow(label: settings.localized("Full Screen"), systemImage: "arrow.up.left.and.arrow.down.right", isOn: $fullScreen)
            .accessibilityHint(settings.localized("Hide system bars and fill the screen"))

        OverlayToggleRow(
            label: settings.localized("Hide Menu Button"),
            systemImage: "eye.slash",
            isOn: Binding(
                get: { menuButtonHidden || settings.hideMenuButton },
                set: { newValue in
                    menuButtonHidden = newValue
                    settings.hideMenuButton = newValue
                }
            )
        )
        .accessibilityHint(settings.localized("Tap the game area or press any controller button to show it again"))

        if vmMenuAvailable {
            OverlayActionRow(label: settings.localized("Speed / Fast Forward"), systemImage: "forward.fill") {
                onOpen(.speed)
            }
        }
    }

    @ViewBuilder private var thisGameRows: some View {
        if gameMenuAvailable {
            OverlayActionRow(label: settings.localized("Per-Game Settings"), systemImage: "slider.horizontal.3") {
                onOpen(.perGame)
            }
            .accessibilityHint(settings.localized("Graphics, audio, CPU, pad, and fixes for this title"))
        }
        injectedMenuRow(controllerSkinMenu)
        OverlayActionRow(
            label: settings.localized("Edit Virtual Pad Layout"),
            systemImage: "square.resize",
            trailingValue: activePadLayoutName
        ) {
            onOpen(.padLayout)
        }
    }

    @ViewBuilder private var gameToolsRows: some View {
        if gameMenuAvailable || vmMenuAvailable {
            OverlayActionRow(label: settings.localized("Save / Load States"), systemImage: "square.stack.3d.up.fill") {
                onOpen(.saveStates)
            }
        }
        if vmMenuAvailable {
            injectedMenuRow(discMenu)
        }
        if gameMenuAvailable {
            OverlayActionRow(label: settings.localized("RetroAchievements"), systemImage: "trophy.fill") {
                onOpen(.retroAchievements)
            }
            OverlayActionRow(label: settings.localized("Cheats & Patches"), systemImage: "rectangle.stack.badge.plus") {
                onOpen(.cheats)
            }
        }
    }

    @ViewBuilder private var resetAndExitRows: some View {
        if vmMenuAvailable {
            OverlayActionRow(label: settings.localized("Reset ROM"), systemImage: "arrow.counterclockwise.circle", isDestructive: true) {
                onOpen(.resetROM)
            }
        }
        if gameMenuAvailable {
            OverlayActionRow(label: settings.localized("Clear Current Game Cache"), systemImage: "trash.slash", action: onClearCache)
        }
        OverlayActionRow(label: settings.localized("Back to Menu"), systemImage: "list.bullet", action: onBackToMenu)
            .accessibilityHint(settings.localized("Quits this game and returns to the library"))
    }

    /// Hosts an injected SwiftUI `Menu` (controller skin / change disc) as a row that matches the
    /// graphite action rows as closely as an opaque AnyView allows. The menu's own action
    /// semantics are untouched.
    @ViewBuilder
    private func injectedMenuRow(_ menu: AnyView) -> some View {
        menu
            .foregroundStyle(OverlayTheme.textPrimary)
            .frame(maxWidth: .infinity, minHeight: variant == .phoneLandscape ? 38 : 44, alignment: .leading)
    }
}

private struct LandscapeCommandBar: View {
    let settings: SettingsStore
    let gameTitle: String?
    let onResume: () -> Void
    let iconOnly: Bool

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 10) {
                Image(systemName: "pause.circle.fill")
                    .font(.system(size: 18))
                    .foregroundStyle(OverlayTheme.accent)
                Text(settings.localized("Paused"))
                    .font(.callout.weight(.semibold))
                    .foregroundStyle(OverlayTheme.textPrimary)
                    .layoutPriority(1)
                if let title = gameTitle, !title.isEmpty {
                    Text(title)
                        .font(.caption)
                        .foregroundStyle(OverlayTheme.textSecondary)
                        .lineLimit(1)
                        .truncationMode(.tail)
                        .layoutPriority(-1)
                }
                Spacer(minLength: 8)
                Button(action: onResume) {
                    if iconOnly {
                        Image(systemName: "play.fill")
                    } else {
                        Label(settings.localized("Resume"), systemImage: "play.fill")
                    }
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.small)
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 6)
            OverlayTheme.separator
                .frame(height: 0.5)
        }
    }
}
