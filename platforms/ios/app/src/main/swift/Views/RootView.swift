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

            SafeAreaProtectedMenuTabContent {
                BIOSListView()
            }
                .environment(\.menuTabIsActive, selectedTab == 1)
                .tabItem {
                    Label(settings.localized("BIOS"), systemImage: "cpu")
                }
                .tag(1)

            SafeAreaProtectedMenuTabContent {
                HelpView()
            }
                .environment(\.menuTabIsActive, selectedTab == 2)
                .tabItem {
                    Label(settings.localized("Help"), systemImage: "questionmark.circle")
                }
                .tag(2)

            SafeAreaProtectedMenuTabContent {
                NavigationStack {
                    SettingsRootView()
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
        // SwiftUI's GeometryReader safe-area insets are unreliable inside a TabView
        // page (the left/right notch/Dynamic Island insets read as zero in landscape),
        // so read the real key-window insets instead and use the geometry size only as
        // a reliable signal to recompute them on rotation. This keeps normal app tab
        // content clear of the notch without touching gameplay or overlay surfaces.
        GeometryReader { geometry in
            let insets = NormalTabContentMargin.effectiveHorizontalInsets(
                raw: safeAreaInsets,
                isLandscape: geometry.size.width > geometry.size.height,
                idiom: UIDevice.current.userInterfaceIdiom
            )
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
/// The raw key-window safe-area insets only clear the hardware cutout (notch / Dynamic
/// Island / sensor housing) by the minimum amount, which still leaves tab content cramped
/// against the cutout in landscape. This applies a small minimum content margin on each
/// side so Games, BIOS, Help, and Settings sit in a balanced, readable column. Portrait and
/// iPad keep the raw insets unchanged, and gameplay surfaces never use this helper.
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
