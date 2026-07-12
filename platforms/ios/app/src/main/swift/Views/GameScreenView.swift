// GameScreenView.swift — Unified game screen (Metal + Virtual Pad + Menu)
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit
import GameController

private let runtimeMenuStateChangedNotification = Notification.Name("ARMSX2iOSRuntimeMenuStateChanged")
private let retroAchievementsToastNotification = Notification.Name("ARMSX2RetroAchievementsNotification")

private struct RetroAchievementsToast: Equatable {
    let title: String
    let message: String
    let badgePath: String?
}

private struct RetroAchievementEntry: Identifiable, Equatable {
    let id: Int
    let title: String
    let description: String
    let badgePath: String?
    let measuredProgress: String
    let points: Int
    let unlockTime: Int
    let state: Int
    let category: Int
    let bucket: Int
    let unlocked: Int
    let measuredPercent: Double
    let rarity: Double
    let rarityHardcore: Double

    init?(dictionary: [String: Any]) {
        guard let idNumber = dictionary["id"] as? NSNumber else { return nil }
        id = idNumber.intValue
        title = dictionary["title"] as? String ?? ""
        description = dictionary["description"] as? String ?? ""
        let rawBadgePath = (dictionary["badgePath"] as? String ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        badgePath = rawBadgePath.isEmpty ? nil : rawBadgePath
        measuredProgress = dictionary["measuredProgress"] as? String ?? ""
        points = (dictionary["points"] as? NSNumber)?.intValue ?? 0
        unlockTime = (dictionary["unlockTime"] as? NSNumber)?.intValue ?? 0
        state = (dictionary["state"] as? NSNumber)?.intValue ?? 0
        category = (dictionary["category"] as? NSNumber)?.intValue ?? 0
        bucket = (dictionary["bucket"] as? NSNumber)?.intValue ?? 0
        unlocked = (dictionary["unlocked"] as? NSNumber)?.intValue ?? 0
        measuredPercent = (dictionary["measuredPercent"] as? NSNumber)?.doubleValue ?? 0
        rarity = (dictionary["rarity"] as? NSNumber)?.doubleValue ?? 0
        rarityHardcore = (dictionary["rarityHardcore"] as? NSNumber)?.doubleValue ?? 0
    }

    var isUnlocked: Bool { state == 2 }
    var isUnsupported: Bool { state == 3 || bucket == 3 }
    var isUnofficial: Bool { (category & 2) != 0 || bucket == 4 }
    var isActiveChallenge: Bool { bucket == 6 || bucket == 7 }
}

private struct GameScreenSizePreferenceKey: PreferenceKey {
    static let defaultValue: CGSize = .zero
    static func reduce(value: inout CGSize, nextValue: () -> CGSize) {
        value = nextValue()
    }
}

/// Single source of truth for the in-game overlay stack, replacing the previous set of
/// independent per-screen `@State` booleans.
///
/// - `.hidden`: gameplay; the VM is running and no overlay is up.
/// - `.paused`: the pause-menu card is visible.
/// - `.pausedPresenting`: the pause menu is logically open *underneath* a child screen (a
///   sheet, the pad-layout overlay, the per-game settings overlay, or the reset alert). The
///   pause card itself is not rendered in this state; the child covers the screen.
///
/// Dismissing any pause-launched child returns to `.paused` (never to `.hidden`), so closing
/// Save States / Per-Game Settings / Cheats / RetroAchievements / Pad Layout / Reset ROM lands
/// back on the pause menu instead of resuming gameplay. `Resume`, Back to Menu, Reset ROM and
/// restart-with-disc are the only intentional paths to `.hidden`.
private enum OverlayRoute: Equatable {
    case hidden
    case paused
    case pausedPresenting(QuickMenuDestination)
}

struct GameScreenView: View {
    // MARK: - State & Constants

    @State private var appState = AppState.shared
    @State private var settings = SettingsStore.shared
    @State private var layoutPresets = PadLayoutPresetStore.shared
    @State private var skinLibrary = VPadSkinLibraryStore.shared
    @State private var userVirtualPadVisible = true
    @State private var externalControllerConnected = false
    @State private var fullScreen = false
    @State private var menuButtonHidden = false
    @State private var vmMenuAvailable = false
    @State private var gameMenuAvailable = false
    // MARK: Overlay Route
    // The pause card + every screen launched from it are driven by one FSM. Opening a child
    // transitions `.paused -> .pausedPresenting(child)` without tearing the card down; the child
    // covers the screen and dismissing it returns to `.paused` (the pause menu), not gameplay.
    @State private var overlayRoute: OverlayRoute = .hidden
    @State private var runtimePerGameSettingsEntry: ISOEntry?
    @State private var runtimePerGameSettings: [String: Any]?
    @State private var runtimePadLayoutIdentity: PadLayoutGameIdentity?
    // Auto-dismissing banner controllers. Status uses the brief duration as its
    // default (important messages override per-call); achievements use 5s. Both
    // preserve the original easeOut/easeIn 0.18s show/hide and generation-bump
    // cancel semantics (see TransientBannerController).
    @StateObject private var statusBanner = TransientBannerController<String>(defaultDisplayDuration: Self.briefStatusDisplayDuration)
    @StateObject private var achievementsBanner = TransientBannerController<RetroAchievementsToast>(defaultDisplayDuration: Self.retroAchievementsToastDisplayDuration, queuesConcurrentPresentations: true)
    @State private var runtimeOverlayPauseActive = false
    @State private var previousHideHomeIndicator = false
    @State private var previousHideStatusBar = false
    @State private var wasBackgrounded = false
    // Bumped whenever the in-game pad editor dismisses, so the gameplay controller is
    // rebuilt from scratch (fresh UIKit press surfaces) instead of diffed. This avoids
    // stale UIControl/hosting-controller state left behind by visibility edits.
    @State private var padRebuildToken = 0
    // Polls external controllers for any button/stick activity while the menu button is
    // hidden, so external-controller-only users are never softlocked out of pause. The
    // poll reads GCController state snapshots only (no handlers), so it cannot steal input
    // from SDL/core. Started when the menu is hidden during gameplay, stopped on restore.
    @State private var menuRestorePollTimer: Timer?
    @State private var lastControllerInputActive = false

    @Environment(\.scenePhase) private var scenePhase
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    private static let briefStatusDisplayDuration: TimeInterval = 2.2
    private static let importantStatusDisplayDuration: TimeInterval = 6.0
    private static let retroAchievementsToastDisplayDuration: TimeInterval = 5.0

    private var displaySafeAreaInsets: UIEdgeInsets {
        UIApplication.shared.connectedScenes
            .compactMap { $0 as? UIWindowScene }
            .first?.windows
            .first?.safeAreaInsets ?? .zero
    }

    /// SwiftUI `EdgeInsets` view of `displaySafeAreaInsets`, for the shared overlay
    /// container which bounds its card from the host window's safe-area insets so the
    /// card clears the notch / Dynamic Island / home indicator.
    private var displaySafeAreaEdgeInsets: EdgeInsets {
        let insets = displaySafeAreaInsets
        return EdgeInsets(top: insets.top, leading: insets.left, bottom: insets.bottom, trailing: insets.right)
    }

    @ViewBuilder
    private var pauseMenuOverlay: some View {
        if case .paused = overlayRoute {
            // The pause card renders only in `.paused`. While a child is up
            // (`.pausedPresenting`) the card is omitted so it cannot z-order over the
            // child overlay; the child covers the screen and the card reappears on dismiss.
            GameOverlayContainer(
                safeAreaInsets: displaySafeAreaEdgeInsets,
                onTapOutside: { overlayRoute = .hidden },
                frameMode: .landscapePanel
            ) { metrics in
                QuickMenuView(
                    settings: settings,
                    padVisible: $userVirtualPadVisible,
                    fullScreen: $fullScreen,
                    menuButtonHidden: $menuButtonHidden,
                    vmMenuAvailable: vmMenuAvailable,
                    gameMenuAvailable: gameMenuAvailable,
                    virtualPadHiddenByController: virtualPadHiddenByController,
                    gameTitle: currentRuntimeGameName(),
                    controllerSkinMenu: AnyView(controllerSkinMenu),
                    discMenu: AnyView(discSwapMenu),
                    variant: metrics.variant,
                    activePadLayoutName: activePadLayoutDisplayName,
                    onCycleOSD: { cycleOsdPreset() },
                    onOpen: { destination in
                        // Transition to `.pausedPresenting` without closing the card; the
                        // child covers the screen and dismissing it returns to `.paused`.
                        openPauseMenuChild(destination)
                    },
                    onClearCache: {
                        overlayRoute = .hidden
                        clearCurrentGameCache()
                    },
                    onBackToMenu: {
                        overlayRoute = .hidden
                        appState.returnToMenu()
                    },
                    onResume: {
                        if settings.hapticFeedback { HapticManager.light.impactOccurred() }
                        overlayRoute = .hidden
                    }
                )
            }
        }
    }

    // MARK: - Body

    var body: some View {
        GeometryReader { geo in
            let isLandscape = geo.size.width > geo.size.height

            Group {
                if isLandscape {
                    // Landscape: full-screen layout so pad coordinates match the layout editor.
                    ZStack {
                        MetalGameView()
                            .onTapGesture { restoreMenuButtonIfHidden() }
                            .accessibilityElement(children: .ignore)
                            .accessibilityLabel("Game display")
                            .accessibilityAddTraits(.isImage)
                            .accessibilityHint("VoiceOver image recognition can read on-screen text.")
                        AccessibilityHUDMirror()
                        if effectiveVirtualPadVisible {
                            VirtualControllerView(
                                isLandscape: true,
                                layoutSnapshot: effectivePadLayoutSnapshot,
                                skinDescriptor: effectivePadSkinDescriptor
                            )
                            .id(padRebuildToken)
                        }
                        menuButtonOverlay(isLandscape: true)
                    }
                    .ignoresSafeArea()
                } else {
                    // Portrait: top game viewport, bottom controller deck.
                    // Game respects the top safe area so OSD stays below the Dynamic Island.
                    // Controller ignores the bottom safe area so buttons remain usable near the home indicator.
                    VStack(spacing: 0) {
                        let gameHeight = min(geo.size.width * 3 / 4, geo.size.height * 0.6)
                        MetalGameView()
                            .frame(height: gameHeight)
                            .clipped()
                            .onTapGesture { restoreMenuButtonIfHidden() }
                            .accessibilityElement(children: .ignore)
                            .accessibilityLabel("Game display")
                            .accessibilityAddTraits(.isImage)
                            .accessibilityHint("VoiceOver image recognition can read on-screen text.")
                            .overlay { AccessibilityHUDMirror() }

                        if effectiveVirtualPadVisible {
                            ZStack {
                                Color.black
                                VirtualControllerView(
                                    layoutSnapshot: effectivePadLayoutSnapshot,
                                    skinDescriptor: effectivePadSkinDescriptor
                                )
                                .frame(maxWidth: .infinity, maxHeight: .infinity)
                            }
                            .frame(maxWidth: .infinity, maxHeight: .infinity)
                            .id(padRebuildToken)
                        }
                    }
                    .overlay(alignment: .topTrailing) {
                        if !menuButtonHidden {
                            menuButton()
                                .padding(.top, 8)
                                .padding(.trailing, 4)
                        }
                    }
                    .ignoresSafeArea(.container, edges: .bottom)
                }
            }
            .preference(key: GameScreenSizePreferenceKey.self, value: geo.size)
        }
        .onPreferenceChange(GameScreenSizePreferenceKey.self) { _ in
            syncFullscreenStateFromWindow()
        }
        .sheet(isPresented: childPresentedBinding(.saveStates)) {
            SaveStatesPanel { message, isImportant in
                presentStatusMessage(
                    message,
                    displayDuration: isImportant ? Self.importantStatusDisplayDuration : Self.briefStatusDisplayDuration
                )
            }
        }
        .sheet(isPresented: childPresentedBinding(.speed)) {
            SpeedControlPanel(settings: settings)
                .presentationDetents([.medium, .large])
        }
        .sheet(isPresented: childPresentedBinding(.retroAchievements)) {
            RetroAchievementsGamePanel(settings: settings)
                .presentationDetents([.medium, .large])
        }
        .overlay(alignment: .top) {
            if case .pausedPresenting(.padLayout) = overlayRoute {
                PadLayoutEditView(onDismiss: {
                    // Dismissing the pad editor returns to the pause menu, not gameplay.
                    // Bump the rebuild token so the gameplay controller is recreated with
                    // fresh UIKit press surfaces after any visibility/layout change.
                    padRebuildToken &+= 1
                    overlayRoute = .paused
                }, context: runtimePadLayoutEditorContext)
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
            }
        }
        .sheet(isPresented: childPresentedBinding(.cheats)) {
            CheatsPatchesManagerView(
                isoName: ARMSX2Bridge.currentGameISOName() ?? "",
                gameTitle: "",
                launchContext: .inGame
            )
            .presentationDetents([.medium, .large])
        }
        .overlay(alignment: .bottom) {
            statusToastOverlay
        }
        .overlay(alignment: .top) {
            retroAchievementsToastOverlay
        }
        .overlay {
            if case .pausedPresenting(.perGame) = overlayRoute {
                // Presented through the same overlay shell as the pause menu so it stays
                // integrated with gameplay (no system sheet chrome / status bar / Dynamic
                // Island leak). The panel dismisses via Save/Cancel, so the backdrop does
                // not tap-to-dismiss.
                GameOverlayContainer(safeAreaInsets: displaySafeAreaEdgeInsets, frameMode: .landscapePanel) { _ in
                    runtimePerGameSettingsContent
                }
            }
        }
        .overlay {
            pauseMenuOverlay
        }
        .animation(reduceMotion ? nil : .easeInOut(duration: 0.2), value: overlayRoute)
        .alert(settings.localized("Reset ROM?"), isPresented: childPresentedBinding(.resetROM)) {
            Button(settings.localized("Cancel"), role: .cancel) {}
            Button(settings.localized("Reset ROM"), role: .destructive) {
                resetCurrentROM()
            }
        } message: {
            Text(settings.localized("Restart the current game? Unsaved progress will be lost."))
        }
        .onAppear {
            enterGameplaySystemChromeMode()
            syncFullscreenStateFromWindow()
            applyInitialFullscreenPreference()
            refreshExternalControllerConnectionState()
            refreshRuntimeMenuState()
            consumePendingRetroAchievementsToast()
            startMenuRestorePollingIfNeeded()
        }
        .onDisappear {
            statusBanner.cancelDismiss()
            achievementsBanner.cancelDismiss()
            stopMenuRestorePolling()
            leaveGameplaySystemChromeMode()
        }
        // Single chokepoint for runtime pause: VM pause derives only from `overlayRoute`
        // (any non-hidden route keeps the VM paused), so one observer covers every child
        // open/dismiss regardless of which screen it was. This replaces the seven per-screen
        // observers that existed for the old independent booleans.
        .onChange(of: overlayRoute) { _, _ in
            updateRuntimeOverlayPause()
            if overlayRoute != .hidden {
                stopMenuRestorePolling()
            } else {
                startMenuRestorePollingIfNeeded()
            }
        }
        .onChange(of: scenePhase) { _, newPhase in
            if newPhase == .background {
                wasBackgrounded = true
            } else if newPhase == .active {
                syncFullscreenStateFromWindow()
                // Returning to a running game from the background: open the pause menu
                // so resuming is deliberate, not a drop straight back into gameplay.
                if wasBackgrounded && overlayRoute == .hidden {
                    overlayRoute = .paused
                }
                wasBackgrounded = false
            }
        }
        .onChange(of: fullScreen) { _, isEnabled in
            applyFullscreenState(isEnabled)
        }
        .onChange(of: settings.hideMenuButton) { _, isHidden in
            // Keep the runtime menu-button flag in lockstep with the persisted setting so
            // re-enabling it (from the quick menu or settings) restores the button at once.
            if menuButtonHidden != isHidden {
                menuButtonHidden = isHidden
            }
            if isHidden {
                startMenuRestorePollingIfNeeded()
            } else {
                stopMenuRestorePolling()
            }
        }
        .onReceive(NotificationCenter.default.publisher(for: runtimeMenuStateChangedNotification)) { _ in
            refreshRuntimeMenuState()
        }
        .onReceive(NotificationCenter.default.publisher(for: .GCControllerDidConnect)) { _ in
            refreshExternalControllerConnectionState()
        }
        .onReceive(NotificationCenter.default.publisher(for: .GCControllerDidDisconnect)) { _ in
            refreshExternalControllerConnectionState()
        }
        .onReceive(NotificationCenter.default.publisher(for: Notification.Name("ARMSX2iOSPadLayoutEditorDismissed"))) { _ in
            // Safety net for external pad-editor dismissal: return to the pause menu.
            padRebuildToken &+= 1
            overlayRoute = .paused
        }
        .onReceive(NotificationCenter.default.publisher(for: retroAchievementsToastNotification)) { notification in
            _ = ARMSX2Bridge.consumePendingRetroAchievementsNotification()
            presentRetroAchievementsToast(notification)
        }
        .onReceive(Timer.publish(every: 0.5, on: .main, in: .common).autoconnect()) { _ in
            refreshRuntimeMenuState()
        }
        .persistentSystemOverlays(.hidden)
    }

    // MARK: - Layout Views

    @ViewBuilder
    private func menuButtonOverlay(isLandscape: Bool) -> some View {
        if !menuButtonHidden {
            VStack {
                HStack {
                    Spacer()
                    menuButton()
                }
                .padding(.top, isLandscape ? 8 : 4)
                .padding(.trailing, isLandscape ? 8 : 4)
                Spacer()
            }
        }
    }

    private func menuButton() -> some View {
        Button {
            if settings.hapticFeedback { HapticManager.light.impactOccurred() }
            overlayRoute = .paused
        } label: {
            menuButtonLabel
        }
        .accessibilityLabel(settings.localized("Pause Menu"))
        .accessibilityHint(settings.localized("Opens the pause menu"))
    }

    @ViewBuilder
    private var menuButtonLabel: some View {
        // Always-rendered SF Symbol mark. A loose PNG was previously loaded here, but
        // it loaded successfully (so the fallback never ran) while rendering nearly
        // invisible at 30pt over gameplay. Using a template SF Symbol with a strong
        // white foreground guarantees the icon is readable on both dark and bright
        // gameplay regardless of any bundled asset, so the button is never iconless.
        Image(systemName: "pause.circle.fill")
            .font(.system(size: 22, weight: .semibold))
            .symbolRenderingMode(.hierarchical)
            .foregroundStyle(.white)
            .frame(width: 30, height: 30)
            .padding(7)
            .background(.black.opacity(0.40), in: Circle())
    }

    private var controllerSkinMenu: some View {
        Menu {
            ForEach(skinLibrary.allDescriptors) { skin in
                Button {
                    skinLibrary.selectSkin(id: skin.id)
                    settings.virtualPadSkin = skin.virtualPadSkin
                    presentStatusMessage("\(settings.localized("Controller Skin")): \(settings.localized(skin.displayName))")
                } label: {
                    Label(settings.localized(skin.displayName), systemImage: skinLibrary.selectedSkinID == skin.id ? "checkmark" : "circle")
                }
            }
        } label: {
            HStack {
                Label(settings.localized("Controller Skin"), systemImage: "paintpalette")
                Spacer()
                Text(settings.localized(skinLibrary.selectedDescriptor.displayName))
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
                    .truncationMode(.tail)
            }
        }
    }

    private var discSwapMenu: some View {
        Menu {
            Button {
                ejectDisc()
            } label: {
                Label(settings.localized("Eject Disc"), systemImage: "eject")
            }

            let discs = availableDiscSwapNames
            if discs.isEmpty {
                Text(settings.localized("No disc images found"))
            } else {
                Menu {
                    ForEach(discs, id: \.self) { discName in
                        Button {
                            changeDisc(to: discName)
                        } label: {
                            Label(discName, systemImage: "opticaldisc")
                        }
                    }
                } label: {
                    Label(settings.localized("Insert Disc (No Reboot)"), systemImage: "tray.and.arrow.down")
                }

                Menu {
                    ForEach(discs, id: \.self) { discName in
                        Button {
                            restartWithDisc(discName)
                        } label: {
                            Label(discName, systemImage: "arrow.clockwise.circle")
                        }
                    }
                } label: {
                    Label(settings.localized("Restart With Disc"), systemImage: "arrow.clockwise.circle")
                }
            }
        } label: {
            Label(settings.localized("Change Disc"), systemImage: "opticaldisc")
        }
    }

    // MARK: - Runtime Panels

    @ViewBuilder
    private var runtimePerGameSettingsContent: some View {
        if let runtimePerGameSettingsEntry {
            PerGameSettingsPanel(game: runtimePerGameSettingsEntry, preloadedSettings: runtimePerGameSettings, savesToRunningGame: true) {
                closePerGameSettingsOverlay()
            }
        } else {
            NavigationStack {
                ContentUnavailableView(
                    settings.localized("No Game Active"),
                    systemImage: "gamecontroller",
                    description: Text(settings.localized("Start a game before changing per-game settings."))
                )
                .navigationTitle(settings.localized("Per-Game Settings"))
                .toolbar {
                    ToolbarItem(placement: .confirmationAction) {
                        Button(settings.localized("Done")) {
                            closePerGameSettingsOverlay()
                        }
                    }
                }
            }
        }
    }

    // MARK: - Lifecycle & Events

    private func enterGameplaySystemChromeMode() {
        previousHideHomeIndicator = appState.hideHomeIndicator
        previousHideStatusBar = appState.hideStatusBar
        appState.hideHomeIndicator = true
        // Keep the display awake while a game is on screen so it does not sleep mid-play.
        UIApplication.shared.isIdleTimerDisabled = true
    }

    private func leaveGameplaySystemChromeMode() {
        appState.hideHomeIndicator = previousHideHomeIndicator
        appState.hideStatusBar = previousHideStatusBar
        // Allow the screen to auto-sleep again once gameplay ends.
        UIApplication.shared.isIdleTimerDisabled = false
    }

    /// Single source of truth for applying the runtime fullscreen state. Keeps the
    /// SDL window, the SwiftUI status bar policy, and the local toggle in lockstep so
    /// the quick-menu Full Screen toggle takes effect immediately instead of only on
    /// the next app launch.
    private func applyFullscreenState(_ enabled: Bool) {
        ARMSX2Bridge.setFullScreen(enabled)
        if appState.hideStatusBar != enabled {
            appState.hideStatusBar = enabled
        }
    }

    private func applyInitialFullscreenPreference() {
        menuButtonHidden = settings.hideMenuButton
        // Reconcile to the Auto Full Screen preference on every game entry so that
        // changing the setting takes effect on the next boot without an app restart.
        // Previously a stale fullscreen window persisted until the app was relaunched.
        let desired = settings.autoFullscreen
        if fullScreen != desired {
            fullScreen = desired
        }
        applyFullscreenState(desired)
    }

    private func syncFullscreenStateFromWindow() {
        let sdlFullscreen = ARMSX2Bridge.isSDLFullscreen()
        if fullScreen != sdlFullscreen {
            fullScreen = sdlFullscreen
        }
        if appState.hideStatusBar != sdlFullscreen {
            appState.hideStatusBar = sdlFullscreen
        }
    }

    private func restoreMenuButtonIfHidden() {
        guard menuButtonHidden else { return }

        menuButtonHidden = false
        settings.hideMenuButton = false
        stopMenuRestorePolling()
        presentStatusMessage(settings.localized("Menu button shown"))
    }

    /// Starts polling external controllers for any input while the menu button is hidden
    /// and gameplay is active, so a hidden menu can be restored without a screen tap.
    private func startMenuRestorePollingIfNeeded() {
        guard menuButtonHidden, overlayRoute == .hidden, menuRestorePollTimer == nil else { return }
        lastControllerInputActive = controllerInputActive()
        menuRestorePollTimer = Timer.scheduledTimer(withTimeInterval: 0.1, repeats: true) { _ in
            Task { @MainActor in
                guard menuButtonHidden, overlayRoute == .hidden else {
                    stopMenuRestorePolling()
                    return
                }
                let active = controllerInputActive()
                if active && !lastControllerInputActive {
                    restoreMenuButtonIfHidden()
                }
                lastControllerInputActive = active
            }
        }
    }

    private func stopMenuRestorePolling() {
        menuRestorePollTimer?.invalidate()
        menuRestorePollTimer = nil
        lastControllerInputActive = false
    }

    /// Reads a non-destructive snapshot of every external controller's input state. Returns
    /// true if any face button, shoulder, trigger, d-pad direction, thumbstick, or the
    /// menu/options/L3/R3 buttons are currently active. Setting valueChangedHandler would
    /// conflict with SDL; reading these snapshot properties does not.
    private func controllerInputActive() -> Bool {
        for controller in GCController.controllers() {
            guard let gamepad = controller.extendedGamepad else { continue }
            if gamepad.buttonA.isPressed || gamepad.buttonB.isPressed
                || gamepad.buttonX.isPressed || gamepad.buttonY.isPressed {
                return true
            }
            if gamepad.leftShoulder.isPressed || gamepad.rightShoulder.isPressed {
                return true
            }
            if gamepad.leftTrigger.value > 0.1 || gamepad.rightTrigger.value > 0.1 {
                return true
            }
            let dpad = gamepad.dpad
            if dpad.up.isPressed || dpad.down.isPressed || dpad.left.isPressed || dpad.right.isPressed {
                return true
            }
            if abs(gamepad.leftThumbstick.xAxis.value) > 0.1 || abs(gamepad.leftThumbstick.yAxis.value) > 0.1 {
                return true
            }
            if abs(gamepad.rightThumbstick.xAxis.value) > 0.1 || abs(gamepad.rightThumbstick.yAxis.value) > 0.1 {
                return true
            }
            if gamepad.buttonMenu.isPressed {
                return true
            }
            if #available(iOS 13, *), let options = gamepad.buttonOptions, options.isPressed {
                return true
            }
            if #available(iOS 14, *), let l3 = gamepad.leftThumbstickButton, l3.isPressed {
                return true
            }
            if #available(iOS 14, *), let r3 = gamepad.rightThumbstickButton, r3.isPressed {
                return true
            }
        }
        return false
    }

    private func updateRuntimeOverlayPause() {
        // Pause derives centrally from the overlay route: any non-hidden route keeps the VM
        // paused (the pause card, or any child presented from it). `.hidden` is the only state
        // that resumes gameplay, so opening a child and then dismissing it never briefly
        // unpauses the VM the way the old boolean handoff did.
        let shouldPause = overlayRoute != .hidden
        NSLog("@@RUNTIME_OVERLAY_PAUSE@@ should=%d active=%d vm=%d route=%@", shouldPause ? 1 : 0, runtimeOverlayPauseActive ? 1 : 0, ARMSX2Bridge.isVMRunning() ? 1 : 0, String(describing: overlayRoute))
        guard runtimeOverlayPauseActive != shouldPause else { return }

        runtimeOverlayPauseActive = shouldPause
        if ARMSX2Bridge.isVMRunning() {
            ARMSX2Bridge.setVMPaused(shouldPause)
        }
    }

    /// Routes a pause-menu destination to the overlay FSM. `.perGame` needs the VM-safe
    /// settings load, so it goes through `openPerGameSettingsForCurrentGame`; every other
    /// destination simply transitions to `.pausedPresenting` so the card stays logically open
    /// underneath the child and reappears when the child is dismissed.
    private func openPauseMenuChild(_ destination: QuickMenuDestination) {
        // Exhaustive (no `default`): adding a new QuickMenuDestination case without a matching
        // presentation would fail to compile here, so a destination can never silently route to
        // `.pausedPresenting` with no view presenting it.
        switch destination {
        case .perGame:
            openPerGameSettingsForCurrentGame()
        case .speed, .saveStates, .cheats, .retroAchievements, .padLayout, .resetROM:
            overlayRoute = .pausedPresenting(destination)
        }
    }

    /// Drives a `.sheet` / `.alert(isPresented:)` from the single overlay route. `get`
    /// presents the child when the route is `.pausedPresenting(child)`; `set(false)` on
    /// dismissal returns to `.paused` — *unless* a teardown path (Reset ROM / restart-with-disc)
    /// has already moved the route to `.hidden`, in which case the dismissal is a no-op so it
    /// does not clobber the intentional return to gameplay.
    private func childPresentedBinding(_ child: QuickMenuDestination) -> Binding<Bool> {
        Binding(
            get: { overlayRoute == .pausedPresenting(child) },
            set: { isPresented in
                guard !isPresented else { return }
                if case .pausedPresenting(let active) = overlayRoute, active == child {
                    overlayRoute = .paused
                }
            }
        )
    }

    private func refreshRuntimeMenuState() {
        let vmRunning = ARMSX2Bridge.isVMRunning()
        let gameReady = ARMSX2Bridge.hasValidSaveStateGame()
        if vmMenuAvailable != vmRunning {
            vmMenuAvailable = vmRunning
        }
        if gameMenuAvailable != gameReady {
            gameMenuAvailable = gameReady
        }
        let identity = runtimePadLayoutIdentityForCurrentGame()
        if runtimePadLayoutIdentity != identity {
            runtimePadLayoutIdentity = identity
        }
    }

    private func refreshExternalControllerConnectionState() {
        let connected = !GCController.controllers().isEmpty
        if externalControllerConnected != connected {
            externalControllerConnected = connected
        }
    }

    // MARK: - Game Identity Helpers

    private func currentRuntimeGameName() -> String? {
        if let gameName = normalizedRuntimeGameName(appState.runningGameName) {
            return gameName
        }

        if let gameName = normalizedRuntimeGameName(ARMSX2Bridge.currentGameISOName()) {
            return gameName
        }

        if let gameName = normalizedRuntimeGameName(ARMSX2Bridge.currentISOPath()) {
            return gameName
        }

        let bootISO = ARMSX2Bridge.getINIString("GameISO", key: "BootISO", defaultValue: "")
        if let gameName = normalizedRuntimeGameName(bootISO) {
            return gameName
        }

        return gameNameMatchingRuntimeIdentity()
    }

    private func normalizedRuntimeGameName(_ value: String?) -> String? {
        guard var value = value?.trimmingCharacters(in: .whitespacesAndNewlines),
              !value.isEmpty else {
            return nil
        }

        value = value.trimmingCharacters(in: CharacterSet(charactersIn: "\"'"))
        let fileName = (value as NSString).lastPathComponent
        guard !fileName.isEmpty,
              fileName != "BIOS",
              fileName != "AutoBoot" else {
            return nil
        }

        return fileName
    }

    private func gameNameMatchingRuntimeIdentity() -> String? {
        let identity = normalizedRuntimeIdentity(ARMSX2Bridge.compatibilityIdentityForCurrentGame())
        guard !identity.isEmpty else {
            return nil
        }

        for gameName in ARMSX2Bridge.availableISOs() {
            let metadata = ARMSX2Bridge.gameMetadata(forISO: gameName)
            let serial = normalizedRuntimeIdentity(metadata["serial"])
            if !serial.isEmpty && serial == identity {
                return gameName
            }

            if let crc = metadata["crc"]?.trimmingCharacters(in: .whitespacesAndNewlines).uppercased(),
               !crc.isEmpty,
               (identity == crc || identity == "CRC-\(crc)") {
                return gameName
            }
        }

        return nil
    }

    private func normalizedRuntimeIdentity(_ value: String?) -> String {
        (value ?? "")
            .replacingOccurrences(of: "_", with: "-")
            .trimmingCharacters(in: .whitespacesAndNewlines)
            .uppercased()
    }

    // MARK: - Actions

    private func openPerGameSettingsForCurrentGame() {
        // Use the VM-safe bridge path to avoid a disc-image scan while the game is running.
        guard let gameName = currentRuntimeGameName(),
              let info = ARMSX2Bridge.gameSettingsForCurrentGame() else {
            runtimePerGameSettingsEntry = nil
            runtimePerGameSettings = nil
            withAnimation(.spring(response: 0.32, dampingFraction: 0.88)) {
                overlayRoute = .pausedPresenting(.perGame)
            }
            presentImportantStatusMessage(settings.localized("Per-game settings need a running game."))
            return
        }

        let serial = (info["serial"] as? String)?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        runtimePerGameSettingsEntry = ISOEntry(
            name: gameName,
            fileURL: nil,
            bootPath: nil,
            coverURL: nil,
            coverSignature: nil,
            metadata: serial.isEmpty ? [:] : ["serial": serial],
            size: 0,
            isFavorite: false
        )
        runtimePerGameSettings = info
        withAnimation(.spring(response: 0.32, dampingFraction: 0.88)) {
            overlayRoute = .pausedPresenting(.perGame)
        }
    }

    private func closePerGameSettingsOverlay() {
        // Save/Cancel from Per-Game Settings return to the pause menu, not gameplay.
        withAnimation(.easeInOut(duration: 0.2)) {
            overlayRoute = .paused
        }
        runtimePerGameSettingsEntry = nil
        runtimePerGameSettings = nil
        refreshRuntimeMenuState()
    }

    private func resetCurrentROM() {
        // Drop out of the overlay before tearing the VM down so the pause state cannot
        // re-pause a resetting VM.
        overlayRoute = .hidden
        appState.resetCurrentVM()
        presentStatusMessage(settings.localized("Restarting ROM..."))
    }

    private func clearCurrentGameCache() {
        guard let gameName = currentRuntimeGameName() else {
            presentImportantStatusMessage(settings.localized("Cache clear needs a running game."))
            return
        }

        let message = ARMSX2Bridge.clearCache(forISO: gameName)
        presentStatusMessage(message)
    }

    private func changeDisc(to discName: String) {
        presentStatusMessage("Changing disc...")
        ARMSX2Bridge.changeDisc(toISO: discName) { success in
            Task { @MainActor in
                if success {
                    presentStatusMessage("\(discName) inserted. Use the game's disc-swap prompt if needed.")
                } else {
                    presentImportantStatusMessage("Could not change discs. Open the game's disc-swap prompt first, or restart with the target disc.")
                }
            }
        }
    }

    private func restartWithDisc(_ discName: String) {
        // Drop out of the overlay before the VM shutdown/boot so the pause state cannot
        // fight the teardown.
        overlayRoute = .hidden
        presentStatusMessage("Restarting with \(discName)...")
        appState.shutdownAndBoot(isoName: discName)
    }

    private func ejectDisc() {
        presentStatusMessage("Ejecting disc...")
        ARMSX2Bridge.ejectDisc { success in
            Task { @MainActor in
                if success {
                    presentStatusMessage("Disc ejected")
                } else {
                    presentImportantStatusMessage("Could not eject the disc. Try again after the game has finished loading.")
                }
            }
        }
    }

    // MARK: - Toast & Feedback

    @ViewBuilder
    private var statusToastOverlay: some View {
        if let statusMessage = statusBanner.content {
            Text(statusMessage)
                .font(.callout.weight(.semibold))
                .foregroundStyle(.white)
                .padding(.horizontal, 14)
                .padding(.vertical, 10)
                .background(.black.opacity(0.72), in: Capsule())
                .padding(.bottom, 24)
                .padding(.horizontal, max(max(displaySafeAreaInsets.left, displaySafeAreaInsets.right), 14))
                .transition(.opacity.combined(with: .move(edge: .bottom)))
        }
    }

    @ViewBuilder
    private var retroAchievementsToastOverlay: some View {
        if let retroAchievementsToast = achievementsBanner.content {
            HStack(spacing: 12) {
                retroAchievementsBadge(path: retroAchievementsToast.badgePath)

                VStack(alignment: .leading, spacing: 2) {
                    Text("RetroAchievements")
                        .font(.caption.weight(.bold))
                        .foregroundStyle(.yellow)
                    Text(retroAchievementsToast.title)
                        .font(.callout.weight(.semibold))
                        .foregroundStyle(.white)
                        .lineLimit(1)
                    if !retroAchievementsToast.message.isEmpty {
                        Text(retroAchievementsToast.message)
                            .font(.caption)
                            .foregroundStyle(.white.opacity(0.82))
                            .lineLimit(2)
                    }
                }

                Spacer(minLength: 0)
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 10)
            .frame(maxWidth: 390)
            .background(.black.opacity(0.78), in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            .overlay {
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .stroke(.white.opacity(0.12), lineWidth: 1)
            }
            .padding(.top, max(displaySafeAreaInsets.top, 54))
            .padding(.horizontal, max(max(displaySafeAreaInsets.left, displaySafeAreaInsets.right), 14))
            .allowsHitTesting(false)
            .transition(.opacity.combined(with: .move(edge: .top)))
        }
    }

    @ViewBuilder
    private func retroAchievementsBadge(path: String?) -> some View {
        if let path,
           let image = UIImage(contentsOfFile: path) {
            Image(uiImage: image)
                .resizable()
                .scaledToFit()
                .frame(width: 42, height: 42)
                .clipShape(RoundedRectangle(cornerRadius: 8, style: .continuous))
        } else {
            Image(systemName: "trophy.fill")
                .font(.system(size: 22, weight: .semibold))
                .foregroundStyle(.yellow)
                .frame(width: 42, height: 42)
                .background(.white.opacity(0.12), in: RoundedRectangle(cornerRadius: 8, style: .continuous))
        }
    }

    private func presentStatusMessage(
        _ message: String,
        displayDuration: TimeInterval = Self.briefStatusDisplayDuration
    ) {
        statusBanner.present(message, displayDuration: displayDuration)
    }

    private func cycleOsdPreset() {
        let allPresets: [OsdPreset] = OsdPreset.allCases
        guard let currentIndex = allPresets.firstIndex(of: settings.osdPreset) else {
            return
        }
        let nextIndex = (currentIndex + 1) % allPresets.count
        let nextPreset = allPresets[nextIndex]

        settings.osdPreset = nextPreset
        ARMSX2Bridge.setPerformanceOverlayVisible(nextPreset != .off)

        let label = nextPreset != .off
            ? settings.localized(nextPreset.label)
            : settings.localized("OFF")
        presentStatusMessage("OSD: \(label)")
    }

    private func presentRetroAchievementsToast(_ notification: Notification) {
        if (notification.userInfo?["handledByUIKit"] as? Bool) == true {
            return
        }

        let title = ((notification.userInfo?["title"] as? String) ?? "")
            .trimmingCharacters(in: .whitespacesAndNewlines)
        guard !title.isEmpty else { return }

        let message = ((notification.userInfo?["message"] as? String) ?? "")
            .trimmingCharacters(in: .whitespacesAndNewlines)
        let badgePathValue = ((notification.userInfo?["badgePath"] as? String) ?? "")
            .trimmingCharacters(in: .whitespacesAndNewlines)
        let toast = RetroAchievementsToast(
            title: title,
            message: message,
            badgePath: badgePathValue.isEmpty ? nil : badgePathValue
        )

        achievementsBanner.present(toast)
    }

    private func consumePendingRetroAchievementsToast() {
        guard let userInfo = ARMSX2Bridge.consumePendingRetroAchievementsNotification(), !userInfo.isEmpty else { return }
        let notificationUserInfo = Dictionary(uniqueKeysWithValues: userInfo.map { (AnyHashable($0.key), $0.value) })
        presentRetroAchievementsToast(Notification(name: retroAchievementsToastNotification, object: nil, userInfo: notificationUserInfo))
    }

    private func presentImportantStatusMessage(_ message: String) {
        presentStatusMessage(message, displayDuration: Self.importantStatusDisplayDuration)
    }

    // MARK: - Virtual Pad

    private var effectiveVirtualPadVisible: Bool {
        userVirtualPadVisible && (!settings.autoHideVirtualPadWhenControllerConnected || !externalControllerConnected) && overlayRoute != .pausedPresenting(.padLayout)
    }

    private var effectivePadLayoutSnapshot: PadLayoutSnapshot? {
        layoutPresets.effectiveSnapshot(for: runtimePadLayoutIdentity)
    }

    /// Friendly name for the virtual pad layout currently in effect, shown beside the
    /// Edit Virtual Pad Layout row so the active value is visible at a glance.
    private var activePadLayoutDisplayName: String {
        if let preset = layoutPresets.effectivePreset(for: runtimePadLayoutIdentity) {
            return preset.displayName
        }
        return settings.localized("Current Layout")
    }

    private var effectivePadSkinDescriptor: VPadSkinDescriptor {
        layoutPresets.effectiveSkinDescriptor(for: runtimePadLayoutIdentity, using: skinLibrary)
    }

    private var runtimePadLayoutEditorContext: PadLayoutEditorContext {
        let preset = layoutPresets.effectivePreset(for: runtimePadLayoutIdentity)
        let editablePresetID = runtimePadLayoutIdentity.flatMap { layoutPresets.presetID(for: $0) }
            ?? (runtimePadLayoutIdentity == nil ? layoutPresets.globalPresetID : nil)
        return PadLayoutEditorContext(
            presetID: editablePresetID,
            gameIdentity: runtimePadLayoutIdentity,
            initialSnapshot: preset?.snapshot,
            skinDescriptor: effectivePadSkinDescriptor
        )
    }

    private func runtimePadLayoutIdentityForCurrentGame() -> PadLayoutGameIdentity? {
        guard let info = ARMSX2Bridge.gameSettingsForCurrentGame() else {
            return nil
        }
        return PadLayoutGameIdentity(
            serial: info["serial"] as? String,
            crc: info["crc"] as? String
        )
    }

    private var virtualPadHiddenByController: Bool {
        userVirtualPadVisible && settings.autoHideVirtualPadWhenControllerConnected && externalControllerConnected
    }

    // MARK: - Disc Helpers

    private var availableDiscSwapNames: [String] {
        ARMSX2Bridge.availableISOs().filter { !$0.lowercased().hasSuffix(".elf") }
    }
}

// MARK: - RetroAchievements Panel

private struct RetroAchievementsGamePanel: View {
    @Environment(\.dismiss) private var dismiss
    let settings: SettingsStore

    @State private var entries: [RetroAchievementEntry] = []
    @State private var state: [String: Any] = [:]

    var body: some View {
        NavigationStack {
            List {
                summarySection

                if entries.isEmpty {
                    emptySection
                } else {
                    ForEach(groupedEntries, id: \.title) { group in
                        if !group.entries.isEmpty {
                            Section(group.title) {
                                ForEach(group.entries) { entry in
                                    RetroAchievementRow(entry: entry, settings: settings)
                                }
                            }
                        }
                    }
                }
            }
            .navigationTitle(settings.localized("RetroAchievements"))
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button(settings.localized("Done")) {
                        dismiss()
                    }
                }
            }
            .onAppear(perform: refresh)
            .onReceive(NotificationCenter.default.publisher(for: Notification.Name("ARMSX2RetroAchievementsStateChanged"))) { _ in
                refresh()
            }
            .onReceive(NotificationCenter.default.publisher(for: retroAchievementsToastNotification)) { _ in
                refresh()
            }
        }
    }

    private var summarySection: some View {
        Section {
            VStack(alignment: .leading, spacing: 10) {
                HStack(spacing: 12) {
                    gameBadge

                    VStack(alignment: .leading, spacing: 4) {
                        Text(gameTitle)
                            .font(.headline)
                            .lineLimit(2)
                        Text(progressText)
                            .font(.subheadline.monospacedDigit())
                            .foregroundStyle(.secondary)
                    }
                }

                ProgressView(value: progressFraction)
                    .tint(.yellow)

                if !richPresence.isEmpty {
                    Label(richPresence, systemImage: "quote.bubble")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            .padding(.vertical, 4)
        }
    }

    @ViewBuilder
    private var emptySection: some View {
        Section {
            VStack(spacing: 10) {
                Image(systemName: "trophy")
                    .font(.largeTitle)
                    .foregroundStyle(.secondary)
                Text(emptyTitle)
                    .font(.headline)
                    .multilineTextAlignment(.center)
                Text(emptySubtitle)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
            }
            .frame(maxWidth: .infinity)
            .padding(28)
        }
    }

    @ViewBuilder
    private var gameBadge: some View {
        if let path = state["gameIconPath"] as? String,
           !path.isEmpty,
           let image = UIImage(contentsOfFile: path) {
            Image(uiImage: image)
                .resizable()
                .scaledToFit()
                .frame(width: 54, height: 54)
                .clipShape(RoundedRectangle(cornerRadius: 10, style: .continuous))
        } else {
            Image(systemName: "trophy.fill")
                .font(.system(size: 26, weight: .semibold))
                .foregroundStyle(.yellow)
                .frame(width: 54, height: 54)
                .background(.yellow.opacity(0.14), in: RoundedRectangle(cornerRadius: 10, style: .continuous))
        }
    }

    private var gameTitle: String {
        let title = state["gameTitle"] as? String ?? ""
        return title.isEmpty ? settings.localized("Current Game") : title
    }

    private var richPresence: String {
        state["richPresence"] as? String ?? ""
    }

    private var unlockedCount: Int {
        (state["unlockedAchievements"] as? NSNumber)?.intValue ?? entries.filter(\.isUnlocked).count
    }

    private var totalCount: Int {
        (state["totalAchievements"] as? NSNumber)?.intValue ?? entries.filter { !$0.isUnofficial }.count
    }

    private var unlockedPoints: Int {
        (state["unlockedPoints"] as? NSNumber)?.intValue ?? entries.filter(\.isUnlocked).reduce(0) { $0 + $1.points }
    }

    private var totalPoints: Int {
        (state["totalPoints"] as? NSNumber)?.intValue ?? entries.filter { !$0.isUnofficial }.reduce(0) { $0 + $1.points }
    }

    private var progressFraction: Double {
        guard totalCount > 0 else { return 0 }
        return min(1, max(0, Double(unlockedCount) / Double(totalCount)))
    }

    private var progressText: String {
        "\(unlockedCount)/\(totalCount) \(settings.localized("achievements")) · \(unlockedPoints)/\(totalPoints) \(settings.localized("points"))"
    }

    private var emptyTitle: String {
        if (state["loggedIn"] as? NSNumber)?.boolValue == false {
            return settings.localized("RetroAchievements is not logged in.")
        }
        if (state["hasActiveGame"] as? NSNumber)?.boolValue == false {
            return settings.localized("No RetroAchievements game is active.")
        }
        return settings.localized("No achievements found for this game.")
    }

    private var emptySubtitle: String {
        settings.localized("Start a supported game with RetroAchievements enabled, then reopen this panel.")
    }

    private var groupedEntries: [(title: String, entries: [RetroAchievementEntry])] {
        let active = entries.filter { $0.isActiveChallenge && !$0.isUnlocked && !$0.isUnsupported && !$0.isUnofficial }
        let locked = entries.filter { !$0.isUnlocked && !$0.isActiveChallenge && !$0.isUnsupported && !$0.isUnofficial }
        let unlocked = entries.filter { $0.isUnlocked && !$0.isUnofficial }
        let unofficial = entries.filter(\.isUnofficial)
        let unsupported = entries.filter(\.isUnsupported)

        return [
            (settings.localized("Active / Almost There"), active),
            (settings.localized("Locked"), locked),
            (settings.localized("Unlocked"), unlocked),
            (settings.localized("Unofficial"), unofficial),
            (settings.localized("Unsupported"), unsupported),
        ]
    }

    private func refresh() {
        state = ARMSX2Bridge.retroAchievementsState()
        entries = ARMSX2Bridge.retroAchievementsForCurrentGame()
            .compactMap { RetroAchievementEntry(dictionary: $0) }
    }
}

private struct RetroAchievementRow: View {
    let entry: RetroAchievementEntry
    let settings: SettingsStore

    var body: some View {
        HStack(alignment: .top, spacing: 12) {
            badge

            VStack(alignment: .leading, spacing: 5) {
                HStack(alignment: .firstTextBaseline, spacing: 8) {
                    Text(entry.title.isEmpty ? settings.localized("Untitled Achievement") : entry.title)
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(entry.isUnlocked ? .primary : .secondary)
                        .lineLimit(2)
                    Spacer(minLength: 8)
                    Text("\(entry.points) pts")
                        .font(.caption.monospacedDigit().weight(.semibold))
                        .foregroundStyle(.yellow)
                }

                if !entry.description.isEmpty {
                    Text(entry.description)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .lineLimit(3)
                }

                HStack(spacing: 8) {
                    Label(statusText, systemImage: statusIcon)
                        .font(.caption2.weight(.semibold))
                        .foregroundStyle(statusColor)

                    if !entry.measuredProgress.isEmpty {
                        Text(entry.measuredProgress)
                            .font(.caption2.monospacedDigit())
                            .foregroundStyle(.secondary)
                    } else if entry.measuredPercent > 0 && entry.measuredPercent < 100 {
                        Text("\(Int(entry.measuredPercent.rounded()))%")
                            .font(.caption2.monospacedDigit())
                            .foregroundStyle(.secondary)
                    }

                    Spacer(minLength: 0)
                }
            }
        }
        .padding(.vertical, 4)
    }

    @ViewBuilder
    private var badge: some View {
        if let path = entry.badgePath,
           let image = UIImage(contentsOfFile: path) {
            Image(uiImage: image)
                .resizable()
                .scaledToFit()
                .frame(width: 46, height: 46)
                .clipShape(RoundedRectangle(cornerRadius: 8, style: .continuous))
                .opacity(entry.isUnlocked ? 1 : 0.55)
        } else {
            Image(systemName: entry.isUnlocked ? "trophy.fill" : "lock.fill")
                .font(.system(size: 22, weight: .semibold))
                .foregroundStyle(entry.isUnlocked ? .yellow : .secondary)
                .frame(width: 46, height: 46)
                .background(.secondary.opacity(0.12), in: RoundedRectangle(cornerRadius: 8, style: .continuous))
        }
    }

    private var statusText: String {
        if entry.isUnsupported { return settings.localized("Unsupported") }
        if entry.isUnofficial { return settings.localized("Unofficial") }
        if entry.isUnlocked { return settings.localized("Unlocked") }
        if entry.isActiveChallenge { return settings.localized("Active") }
        return settings.localized("Locked")
    }

    private var statusIcon: String {
        if entry.isUnsupported { return "exclamationmark.triangle.fill" }
        if entry.isUnofficial { return "sparkles" }
        if entry.isUnlocked { return "checkmark.seal.fill" }
        if entry.isActiveChallenge { return "flame.fill" }
        return "lock.fill"
    }

    private var statusColor: Color {
        if entry.isUnsupported { return .orange }
        if entry.isUnofficial { return .purple }
        if entry.isUnlocked { return .green }
        if entry.isActiveChallenge { return .red }
        return .secondary
    }
}

// MARK: - Save States Panel

private struct SaveStatesPanel: View {
    @Environment(\.dismiss) private var dismiss
    @State private var settings = SettingsStore.shared
    @State private var slots: [ARMSX2SaveStateSlotInfo] = []
    @State private var busySlot: Int? = nil
    @State private var pendingOverwrite: ARMSX2SaveStateSlotInfo? = nil
    @State private var hardcoreActive = false

    let statusHandler: (String, Bool) -> Void

    var body: some View {
        NavigationStack {
            ScrollView {
                if slots.isEmpty {
                    VStack(spacing: 10) {
                        Image(systemName: "hourglass")
                            .font(.largeTitle)
                            .foregroundStyle(.secondary)
                        Text(settings.localized("Save states are not ready yet."))
                            .font(.headline)
                        Text(settings.localized("Wait until the game has fully identified, then try again."))
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .multilineTextAlignment(.center)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(32)
                } else {
                    LazyVStack(spacing: 10) {
                        ForEach(slots, id: \.slot) { slot in
                            SaveStateSlotRow(
                                info: slot,
                                isBusy: busySlot == slot.slot,
                                onSave: { save(slot) },
                                onLoad: { load(slot) },
                                onOverwrite: { pendingOverwrite = slot },
                                loadDisabled: hardcoreActive,
                                settings: settings
                            )
                        }
                    }
                    .padding()
                }
            }
            .safeAreaInset(edge: .top) {
                Text(settings.localized(hardcoreActive ?
                    "Hardcore mode allows saving states for debugging, but loading states is blocked." :
                    "Empty slots can save. Occupied slots can load or overwrite."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(.horizontal)
                    .padding(.vertical, 8)
                    .background(.regularMaterial)
            }
            .navigationTitle(settings.localized("Save / Load States"))
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Button(settings.localized("Done")) {
                        dismiss()
                    }
                }
            }
            .onAppear(perform: refresh)
            .onReceive(NotificationCenter.default.publisher(for: runtimeMenuStateChangedNotification)) { _ in
                refresh()
            }
            .onReceive(NotificationCenter.default.publisher(for: Notification.Name("ARMSX2RetroAchievementsStateChanged"))) { _ in
                refresh()
            }
            .confirmationDialog(
                "\(settings.localized("Overwrite Slot")) \(pendingOverwrite?.slot ?? 0)?",
                isPresented: Binding(
                    get: { pendingOverwrite != nil },
                    set: { newValue in
                        if !newValue {
                            pendingOverwrite = nil
                        }
                    }
                ),
                titleVisibility: .visible
            ) {
                Button(settings.localized("Overwrite"), role: .destructive) {
                    if let pendingOverwrite {
                        save(pendingOverwrite)
                    }
                    pendingOverwrite = nil
                }
                Button(settings.localized("Cancel"), role: .cancel) {
                    pendingOverwrite = nil
                }
            }
        }
    }

    private func refresh() {
        slots = ARMSX2Bridge.saveStateSlots()
        hardcoreActive = ARMSX2Bridge.isRetroAchievementsHardcoreActive()
    }

    private func save(_ slot: ARMSX2SaveStateSlotInfo) {
        let slotNumber = slot.slot
        busySlot = slotNumber
        ARMSX2Bridge.saveState(toSlot: slotNumber) { success in
            Task { @MainActor in
                busySlot = nil
                refresh()
                let message = success
                    ? "\(settings.localized("State saved to slot")) \(slotNumber)"
                    : "\(settings.localized("Could not save slot")) \(slotNumber). \(settings.localized("Try again after gameplay has fully loaded."))"
                statusHandler(message, !success)
            }
        }
    }

    private func load(_ slot: ARMSX2SaveStateSlotInfo) {
        guard !hardcoreActive else {
            statusHandler(settings.localized("Hardcore mode blocks loading save states."), true)
            return
        }

        let slotNumber = slot.slot
        busySlot = slotNumber
        ARMSX2Bridge.loadState(fromSlot: slotNumber) { success in
            Task { @MainActor in
                busySlot = nil
                refresh()
                let message = success
                    ? "\(settings.localized("State loaded from slot")) \(slotNumber)"
                    : "\(settings.localized("Could not load slot")) \(slotNumber). \(settings.localized("Make sure it has a saved state first."))"
                statusHandler(message, !success)
                if success {
                    dismiss()
                }
            }
        }
    }
}

// MARK: - Speed Control Panel

private struct SpeedControlPanel: View {
    @Bindable var settings: SettingsStore
    @Environment(\.dismiss) private var dismiss
    @State private var hardcoreActive = false

    var body: some View {
        NavigationStack {
            Form {
                Section(settings.localized("Fast Forward")) {
                    Toggle(settings.localized("Enable Fast Forward"), isOn: Binding(
                        get: { settings.fastForwardRuntimeEnabled },
                        set: { enabled in
                            settings.setRuntimeFastForwardEnabled(enabled)
                        }
                    ))

                    VStack(alignment: .leading, spacing: 10) {
                        HStack {
                            Text(settings.localized("Fast Forward Speed"))
                            Spacer()
                            Text(Self.formatPercent(settings.fastForwardScalar))
                                .foregroundStyle(.secondary)
                                .font(.callout.monospacedDigit())
                        }

                        Slider(
                            value: $settings.fastForwardScalar,
                            in: SettingsStore.minFastForwardScalar...SettingsStore.maxFastForwardScalar,
                            step: 0.25
                        )

                        HStack {
                            quickFastForwardButton(1.5)
                            quickFastForwardButton(2.0)
                            quickFastForwardButton(3.0)
                            quickFastForwardButton(5.0)
                            quickFastForwardButton(10.0)
                        }
                    }
                }

                Section(settings.localized("Frame Limiter")) {
                    Toggle(settings.localized("Enable Limiter"), isOn: Binding(
                        get: { settings.frameLimiterEnabled },
                        set: { enabled in
                            settings.frameLimiterEnabled = enabled
                            enforceHardcoreSpeedFloorIfNeeded()
                        }
                    ))

                    if settings.frameLimiterEnabled {
                        VStack(alignment: .leading, spacing: 10) {
                            HStack {
                                Text(settings.localized("FPS Target"))
                                Spacer()
                                Text(Self.formatFPS(settings.targetFPS))
                                    .foregroundStyle(.secondary)
                                    .font(.callout.monospacedDigit())
                            }

                            Slider(
                                value: Binding(
                                    get: { settings.targetFPS },
                                    set: { value in
                                        settings.targetFPS = value
                                        enforceHardcoreSpeedFloorIfNeeded()
                                    }
                                ),
                                in: SettingsStore.minTargetFPS...SettingsStore.maxTargetFPS,
                                step: 1.0
                            )

                            HStack {
                                quickTargetButton(30)
                                quickTargetButton(45)
                                quickTargetButton(60)
                                quickTargetButton(90)
                                quickTargetButton(120)
                            }
                        }
                    } else {
                        Text(settings.localized("Limiter is OFF. Games can run above normal speed and may draw more power."))
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }

                if hardcoreActive {
                    Section(settings.localized("Hardcore Mode")) {
                        Text(settings.localized("Hardcore mode blocks slowdown and frame advance. Fast forward stays available; Normal Speed is locked at 100% or higher."))
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }

                Section(settings.localized("How It Works")) {
                    Text(settings.localized("This controls PCSX2 Normal Speed. On NTSC games, 60 FPS is normal speed and 30 FPS is about 50% speed. It is safe to change while a game is running."))
                        .font(.caption)
                        .foregroundStyle(.secondary)

                    HStack {
                        Text(settings.localized("Normal Speed"))
                        Spacer()
                        Text(Self.formatPercent(settings.targetFPS / max(settings.ntscFramerate, 1.0)))
                            .foregroundStyle(.secondary)
                            .font(.callout.monospacedDigit())
                    }
                }
            }
            .navigationTitle(settings.localized("Speed / Fast Forward"))
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button(settings.localized("Done")) {
                        dismiss()
                    }
                }
            }
            .onAppear {
                refreshRuntimeState()
            }
            .onReceive(NotificationCenter.default.publisher(for: Notification.Name("ARMSX2RetroAchievementsStateChanged"))) { _ in
                refreshRuntimeState()
            }
        }
    }

    private func refreshRuntimeState() {
        settings.fastForwardRuntimeEnabled = ARMSX2Bridge.limiterMode() == 1
        hardcoreActive = ARMSX2Bridge.isRetroAchievementsHardcoreActive()
        enforceHardcoreSpeedFloorIfNeeded()
    }

    private func enforceHardcoreSpeedFloorIfNeeded() {
        guard hardcoreActive else { return }
        let minimumFPS = settings.ntscFramerate
        if settings.frameLimiterEnabled && settings.targetFPS < minimumFPS {
            settings.targetFPS = minimumFPS
        }
    }

    private func quickFastForwardButton(_ scalar: Float) -> some View {
        Button(Self.formatPercent(scalar)) {
            settings.fastForwardScalar = scalar
        }
        .buttonStyle(.bordered)
        .font(.caption.monospacedDigit())
    }

    private func quickTargetButton(_ fps: Float) -> some View {
        Button(Self.formatCompactFPS(fps)) {
            settings.frameLimiterEnabled = true
            settings.targetFPS = fps
            enforceHardcoreSpeedFloorIfNeeded()
        }
        .disabled(hardcoreActive && fps < settings.ntscFramerate)
        .buttonStyle(.bordered)
        .font(.caption.monospacedDigit())
    }

    private static func formatFPS(_ value: Float) -> String {
        String(format: "%.0f FPS", value)
    }

    private static func formatCompactFPS(_ value: Float) -> String {
        String(format: "%.0f", value)
    }

    private static func formatPercent(_ scalar: Float) -> String {
        String(format: "%.0f%%", scalar * 100.0)
    }
}

// MARK: - Save State Slot Row

private struct SaveStateSlotRow: View {
    let info: ARMSX2SaveStateSlotInfo
    let isBusy: Bool
    let onSave: () -> Void
    let onLoad: () -> Void
    let onOverwrite: () -> Void
    let loadDisabled: Bool
    let settings: SettingsStore

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 12) {
                SaveStatePreview(data: info.previewPNGData, occupied: info.occupied)
                    .frame(width: 96, height: 72)

                VStack(alignment: .leading, spacing: 4) {
                    Text("\(settings.localized("Slot")) \(info.slot)")
                        .font(.headline)

                    if info.occupied {
                        if let modifiedDate = info.modifiedDate {
                            Text(Self.dateFormatter.string(from: modifiedDate))
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                        Text(info.fileName)
                            .font(.caption2)
                            .foregroundStyle(.secondary)
                            .lineLimit(1)
                    } else {
                        Text(settings.localized("Empty"))
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }

                Spacer(minLength: 8)

                if isBusy {
                    ProgressView()
                        .frame(width: 88)
                } else if !info.occupied {
                    Button(action: onSave) {
                        Label(settings.localized("Save"), systemImage: "square.and.arrow.down")
                    }
                    .buttonStyle(.borderedProminent)
                }
            }

            if info.occupied && !isBusy {
                HStack(spacing: 8) {
                    Button(action: onLoad) {
                        Label(settings.localized("Load"), systemImage: "arrow.down.circle")
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(loadDisabled)
                    .frame(maxWidth: .infinity)

                    Button(action: onOverwrite) {
                        Label(settings.localized("Overwrite"), systemImage: "arrow.triangle.2.circlepath")
                    }
                    .buttonStyle(.bordered)
                    .frame(maxWidth: .infinity)
                }
            }
        }
        .padding(12)
        .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
    }

    private static let dateFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateStyle = .medium
        formatter.timeStyle = .short
        return formatter
    }()
}

// MARK: - Save State Preview

private struct SaveStatePreview: View {
    let data: Data?
    let occupied: Bool

    var body: some View {
        ZStack {
            RoundedRectangle(cornerRadius: 6, style: .continuous)
                .fill(.black.opacity(0.12))

            if let data, let image = UIImage(data: data) {
                Image(uiImage: image)
                    .resizable()
                    .scaledToFill()
                    .clipShape(RoundedRectangle(cornerRadius: 6, style: .continuous))
            } else {
                Image(systemName: occupied ? "photo" : "tray")
                    .font(.title2)
                    .foregroundStyle(.secondary)
            }
        }
        .clipped()
    }
}
