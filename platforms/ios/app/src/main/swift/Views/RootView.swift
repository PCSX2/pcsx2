// RootView.swift — Root view switching between menu and game
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit

private struct MenuTabIsActiveEnvironmentKey: EnvironmentKey {
    static let defaultValue = true
}

extension EnvironmentValues {
    var menuTabIsActive: Bool {
        get { self[MenuTabIsActiveEnvironmentKey.self] }
        set { self[MenuTabIsActiveEnvironmentKey.self] = newValue }
    }
}

struct RootView: View {
    @State private var appState = AppState.shared
    @State private var settings = SettingsStore.shared
    @State private var fileImporter = FileImportHandler.shared
    @State private var showBootSplash = true

    var body: some View {
        ZStack {
            switch appState.currentScreen {
            case .menu:
                Color(uiColor: .systemGroupedBackground)
                    .ignoresSafeArea()
                MenuTabView()
            case .playing:
                GameScreenView()
            }

            if showBootSplash {
                BootSplashView {
                    withAnimation(.easeOut(duration: 0.2)) {
                        showBootSplash = false
                    }
                }
                .transition(.opacity)
                .zIndex(100)
            }
        }
        .environment(\.layoutDirection, settings.localizedLayoutDirection)
        .statusBarHidden(showBootSplash)
        .onAppear {
            StikDebugLauncher.autoOpenIfNeeded(reason: "app launch")
        }
        .onOpenURL { url in
            if !ARMSX2DeepLinkHandler.handle(url) {
                fileImporter.handleURL(url)
            }
        }
        .alert(settings.localized("File Import"), isPresented: $fileImporter.showImportAlert) {
            Button(settings.localized("OK")) {}
        } message: {
            Text(fileImporter.lastImportMessage ?? "")
        }
    }
}

struct MenuTabView: View {
    @State private var appState = AppState.shared
    @State private var settings = SettingsStore.shared
    @State private var selectedTab = 0

    private var biosBackgroundActive: Bool { settings.hasCustomBackground && settings.backgroundEnabledInBIOS }
    private var helpBackgroundActive: Bool { settings.hasCustomBackground && settings.backgroundEnabledInHelp }
    private var settingsBackgroundActive: Bool { settings.hasCustomBackground && settings.backgroundEnabledInSettings }

    var body: some View {
#if targetEnvironment(macCatalyst)
        VStack(spacing: 0) {
            CatalystMenuTabBar(selectedTab: $selectedTab)
                .padding(.top, 8)
                .padding(.bottom, 8)

            Group {
                switch selectedTab {
                case 0:
                    GameListView()
                case 1:
                    BIOSListView()
                case 2:
                    HelpView()
                default:
                    SettingsRootView()
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .tint(.blue)
#else
        TabView(selection: $selectedTab) {
            // Games tab is NOT wrapped in SafeAreaProtectedMenuTabContent — it
            // renders its own edge-to-edge custom wallpaper (BackgroundContainerView)
            // inside its NavigationStack ZStack, which must not be clipped by the
            // safe-area padding that the other tabs use.
            GameListView()
                .environment(\.menuTabIsActive, selectedTab == 0)
                .tabItem {
                    Label(settings.localized("Games"), systemImage: "gamecontroller")
                }
                .tag(0)

            // When a tab's background is active it owns its edge-to-edge MenuBackgroundLayer
            // inside its own NavigationStack (matching GameListView), so it must NOT be wrapped
            // in SafeAreaProtectedMenuTabContent — the padding would clip the wallpaper.
            Group {
                if biosBackgroundActive {
                    BIOSListView()
                } else {
                    SafeAreaProtectedMenuTabContent { BIOSListView() }
                }
            }
                .environment(\.menuTabIsActive, selectedTab == 1)
                .tabItem {
                    Label(settings.localized("BIOS"), systemImage: "cpu")
                }
                .tag(1)

            Group {
                if helpBackgroundActive {
                    HelpView()
                } else {
                    SafeAreaProtectedMenuTabContent { HelpView() }
                }
            }
                .environment(\.menuTabIsActive, selectedTab == 2)
                .tabItem {
                    Label(settings.localized("Help"), systemImage: "questionmark.circle")
                }
                .tag(2)

            Group {
                if settingsBackgroundActive {
                    NavigationStack {
                        SettingsRootView()
                    }
                } else {
                    SafeAreaProtectedMenuTabContent {
                        NavigationStack {
                            SettingsRootView()
                        }
                    }
                }
            }
            .environment(\.menuTabIsActive, selectedTab == 3)
            .tabItem {
                Label(settings.localized("Settings"), systemImage: "gearshape")
            }
            .tag(3)
        }
        .tint(.blue)
        .modifier(PreventTabBarCollapseModifier())
#endif
    }
}

/// Forces the tab bar to keep its standard (expanded) appearance on iOS < 26,
/// where the floating tab bar can collapse to a single pill when opaque content
/// extends underneath it. On iOS 26+ the Liquid Glass tab bar handles this
/// automatically, so no override is needed.
private struct PreventTabBarCollapseModifier: ViewModifier {
    func body(content: Content) -> some View {
        if #available(iOS 26, *) {
            content
        } else {
            content.toolbarBackground(.visible, for: .tabBar)
        }
    }
}

@MainActor
private struct SafeAreaProtectedMenuTabContent<Content: View>: View {
    @Environment(\.layoutDirection) private var layoutDirection
    @State private var safeAreaInsets = KeyWindowSafeArea.horizontalInsets()
    let content: Content

    init(@ViewBuilder content: () -> Content) {
        self.content = content()
    }

    var body: some View {
        // Pre-iOS 26, SwiftUI reports no horizontal safe-area inset for a TabView page in
        // landscape, so a bare list slides under the notch and we pad it manually from the
        // key-window insets. On iOS 26+ SwiftUI gets it right, and padding again would
        // double-inset the column. Tabs that draw their own edge-to-edge background skip
        // this wrapper entirely (see MenuTabView).
        GeometryReader { geometry in
            let isLandscapePhone = geometry.size.width > geometry.size.height
                && UIDevice.current.userInterfaceIdiom == .phone
            let systemProvidesInset = geometry.safeAreaInsets.leading > 0
                || geometry.safeAreaInsets.trailing > 0
            let insets: (left: CGFloat, right: CGFloat) = {
                guard isLandscapePhone, !systemProvidesInset else { return (0, 0) }
                return NormalTabContentMargin.effectiveHorizontalInsets(
                    raw: safeAreaInsets,
                    isLandscape: true,
                    idiom: .phone
                )
            }()
            content
                .padding(.leading, layoutDirection == .rightToLeft ? insets.right : insets.left)
                .padding(.trailing, layoutDirection == .rightToLeft ? insets.left : insets.right)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .onChange(of: geometry.size) { _, _ in
                    safeAreaInsets = KeyWindowSafeArea.horizontalInsets()
                }
        }
        .onAppear {
            safeAreaInsets = KeyWindowSafeArea.horizontalInsets()
        }
    }
}

/// Reads the real left/right safe-area insets from the active key window. Used because
/// SwiftUI-reported horizontal safe-area insets are unreliable inside a TabView page.
private enum KeyWindowSafeArea {
    @MainActor
    static func horizontalInsets() -> (left: CGFloat, right: CGFloat) {
        let insets = keyWindowInsets()
        return (insets.left, insets.right)
    }

    @MainActor
    private static func keyWindowInsets() -> UIEdgeInsets {
        for case let scene as UIWindowScene in UIApplication.shared.connectedScenes {
            guard scene.activationState == .foregroundActive || scene.activationState == .foregroundInactive else { continue }
            if let window = scene.windows.first(where: { $0.isKeyWindow }) ?? scene.windows.first {
                return window.safeAreaInsets
            }
        }
        return .zero
    }
}

/// Adds a small readable horizontal margin for normal app tabs in iPhone landscape.
///
/// Only used on the legacy path of `SafeAreaProtectedMenuTabContent` (pre-iOS 26, where
/// SwiftUI reports no horizontal safe-area inset in a TabView page). The raw key-window
/// insets only just clear the notch, so this pads each side a bit more. Portrait, iPad,
/// and gameplay surfaces are unaffected.
private enum NormalTabContentMargin {
    /// Minimum horizontal content margin for normal app tabs on an iPhone in landscape.
    static let minimumLandscapeMargin: CGFloat = 20

    static func effectiveHorizontalInsets(
        raw: (left: CGFloat, right: CGFloat),
        isLandscape: Bool,
        idiom: UIUserInterfaceIdiom
    ) -> (left: CGFloat, right: CGFloat) {
        guard isLandscape, idiom == .phone else { return raw }
        return (max(raw.left, minimumLandscapeMargin), max(raw.right, minimumLandscapeMargin))
    }
}

#if targetEnvironment(macCatalyst)
private struct CatalystMenuTabBar: View {
    @Binding var selectedTab: Int
    @State private var settings = SettingsStore.shared

    private let tabs = [
        (0, "Games"),
        (1, "BIOS"),
        (2, "Help"),
        (3, "Settings"),
    ]

    var body: some View {
        HStack(spacing: 2) {
            ForEach(tabs, id: \.0) { tab in
                Button {
                    selectedTab = tab.0
                } label: {
                    Text(settings.localized(tab.1))
                        .font(.callout)
                        .fontWeight(selectedTab == tab.0 ? .semibold : .regular)
                        .foregroundStyle(.primary)
                        .frame(minWidth: 82)
                        .padding(.vertical, 6)
                        .background {
                            if selectedTab == tab.0 {
                                Capsule()
                                    .fill(Color.primary.opacity(0.12))
                            }
                        }
                }
                .buttonStyle(.plain)

                if tab.0 != tabs.last?.0 {
                    Divider()
                        .frame(height: 20)
                }
            }
        }
        .padding(4)
        .background(.regularMaterial, in: Capsule())
        .shadow(color: .black.opacity(0.08), radius: 18, y: 8)
    }
}
#endif
