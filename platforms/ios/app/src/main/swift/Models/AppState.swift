// AppState.swift — App screen state management
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct EmulationOnlyPresentation: Equatable {
    var showsVirtualControls = false
    var showsQuickMenu = false
    var padLayoutSnapshot: PadLayoutSnapshot?
    var padSkinDescriptor: VPadSkinDescriptor?

    static let minimal = EmulationOnlyPresentation()
}

@Observable
final class AppState: @unchecked Sendable {
    static let shared = AppState()
    static let systemChromeNeedsUpdateNotification = Notification.Name("ARMSX2iOSSystemChromeNeedsUpdate")
    static let releaseMenuBackgroundResourcesNotification = Notification.Name("ARMSX2iOSReleaseMenuBackgroundResources")
    static let emulationOnlyStartupReadyNotification = Notification.Name("ARMSX2iOSEmulationOnlyStartupReady")

    enum Screen {
        case menu
        case playing
    }

    var currentScreen: Screen = .menu
    var selectedTab: Int = 0
    var runningGameName: String? = nil
    var isEmulationOnlyMode: Bool = false
    var emulationOnlyPresentation = EmulationOnlyPresentation.minimal
    private(set) var emulationOnlyStartupReady: Bool = false
    var hideStatusBar: Bool = false {
        didSet {
            if oldValue != hideStatusBar {
                NotificationCenter.default.post(name: Self.systemChromeNeedsUpdateNotification, object: nil)
            }
        }
    }
    var hideHomeIndicator: Bool = false {
        didSet {
            if oldValue != hideHomeIndicator {
                NotificationCenter.default.post(name: Self.systemChromeNeedsUpdateNotification, object: nil)
            }
        }
    }

    @ObservationIgnored private var pendingBootAction: (() -> Void)?
    @ObservationIgnored private var shutdownObserver: NSObjectProtocol?

    @ObservationIgnored private var autoBootObserver: NSObjectProtocol?
    @ObservationIgnored private var emulationOnlyStartupReadyObserver: NSObjectProtocol?

    private init() {
        shutdownObserver = NotificationCenter.default.addObserver(
            forName: NSNotification.Name("ARMSX2iOSVMDidShutdown"),
            object: nil, queue: .main
        ) { [weak self] _ in
            self?.runningGameName = nil
            self?.isEmulationOnlyMode = false
            self?.emulationOnlyPresentation = .minimal
            self?.emulationOnlyStartupReady = false
            if let action = self?.pendingBootAction {
                self?.pendingBootAction = nil
                action()
            } else {
                // No pending reboot — return to menu (VM crash / normal shutdown)
                self?.currentScreen = .menu
            }
        }

        // [P48] Auto-boot: ObjC side posts this notification to switch UI to game screen
        autoBootObserver = NotificationCenter.default.addObserver(
            forName: NSNotification.Name("ARMSX2iOSAutoBootDidStart"),
            object: nil, queue: .main
        ) { [weak self] _ in
            self?.isEmulationOnlyMode = false
            self?.emulationOnlyPresentation = .minimal
            self?.emulationOnlyStartupReady = false
            self?.releaseMenuBackgroundResourcesForGameplay()
            self?.runningGameName = "AutoBoot"
            self?.currentScreen = .playing
        }

        emulationOnlyStartupReadyObserver = NotificationCenter.default.addObserver(
            forName: Self.emulationOnlyStartupReadyNotification,
            object: nil, queue: .main
        ) { [weak self] _ in
            self?.emulationOnlyStartupReady = true
        }
    }

    func bootGame(isoName: String) {
        isEmulationOnlyMode = false
        emulationOnlyPresentation = .minimal
        emulationOnlyStartupReady = false
        releaseMenuBackgroundResourcesForGameplay()
        Task { @MainActor in
            StikDebugLauncher.autoOpenIfNeeded(reason: "game boot")
        }
        ARMSX2Bridge.bootISO(isoName)
        ARMSX2Bridge.prepareGameRenderViewForCurrentRenderer()
        runningGameName = isoName
        currentScreen = .playing
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) {
            ARMSX2Bridge.requestVMBoot()
        }
    }

    func bootBIOSOnly() {
        isEmulationOnlyMode = false
        emulationOnlyPresentation = .minimal
        emulationOnlyStartupReady = false
        releaseMenuBackgroundResourcesForGameplay()
        Task { @MainActor in
            StikDebugLauncher.autoOpenIfNeeded(reason: "BIOS boot")
        }
        ARMSX2Bridge.setINIString("GameISO", key: "BootISO", value: "")
        ARMSX2Bridge.prepareGameRenderViewForCurrentRenderer()
        runningGameName = "BIOS"
        currentScreen = .playing
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) {
            ARMSX2Bridge.requestVMBoot()
        }
    }

    func returnToMenu() {
        if ARMSX2Bridge.isVMRunning() {
            ARMSX2Bridge.setVMPaused(true)
        }
        isEmulationOnlyMode = false
        emulationOnlyPresentation = .minimal
        currentScreen = .menu
        // [P44-2] Restore opaque background on hosting controller
        NotificationCenter.default.post(name: NSNotification.Name("ARMSX2iOSReturnToMenu"), object: nil)
    }

    func returnToGame() {
        if runningGameName != nil {
            isEmulationOnlyMode = false
            emulationOnlyPresentation = .minimal
            releaseMenuBackgroundResourcesForGameplay()
            // [P44-2] Clear background so Metal surface shows through
            NotificationCenter.default.post(name: NSNotification.Name("ARMSX2iOSEnterGameScreen"), object: nil)
            currentScreen = .playing
            ARMSX2Bridge.setVMPaused(false)
        }
    }

    func shutdownAndBoot(isoName: String) {
        pendingBootAction = { [weak self] in
            self?.bootGame(isoName: isoName)
        }
        ARMSX2Bridge.requestVMShutdown()
    }

    func shutdownAndBootBIOS() {
        pendingBootAction = { [weak self] in
            self?.bootBIOSOnly()
        }
        ARMSX2Bridge.requestVMShutdown()
    }

    func resetCurrentVM() {
        guard let runningGameName else { return }

        if runningGameName == "BIOS" {
            shutdownAndBootBIOS()
        } else {
            shutdownAndBoot(isoName: runningGameName)
        }
    }

    /// Permanently removes the in-game SwiftUI controls and menus for the current VM session.
    /// A VM shutdown or a new boot resets this flag and restores the normal gameplay UI.
    func enterEmulationOnlyMode(presentation: EmulationOnlyPresentation) {
        guard case .playing = currentScreen, emulationOnlyStartupReady else { return }
        emulationOnlyPresentation = presentation
        isEmulationOnlyMode = true
    }

    private func releaseMenuBackgroundResourcesForGameplay() {
        NotificationCenter.default.post(
            name: Self.releaseMenuBackgroundResourcesNotification,
            object: nil
        )
    }
}
