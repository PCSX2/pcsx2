// SwiftUIHost.swift — ObjC-callable helper to create SwiftUI hosting controllers
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit

/// Custom hosting controller that respects fullScreen state for status bar hiding
class ARMSX2HostingController<Content: View>: UIHostingController<Content> {
    override var prefersStatusBarHidden: Bool {
        AppState.shared.hideStatusBar
    }
    override var prefersHomeIndicatorAutoHidden: Bool {
        AppState.shared.hideStatusBar || AppState.shared.hideHomeIndicator
    }
    override var preferredStatusBarUpdateAnimation: UIStatusBarAnimation {
        .fade
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(systemChromeNeedsUpdate),
            name: AppState.systemChromeNeedsUpdateNotification,
            object: nil
        )
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(systemChromeNeedsUpdate),
            name: UIApplication.didBecomeActiveNotification,
            object: nil
        )
    }

    deinit {
        NotificationCenter.default.removeObserver(
            self,
            name: AppState.systemChromeNeedsUpdateNotification,
            object: nil
        )
        NotificationCenter.default.removeObserver(
            self,
            name: UIApplication.didBecomeActiveNotification,
            object: nil
        )
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        applyNativeContentScale(to: view)
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        applyNativeContentScale(to: view)
    }

    @objc private func systemChromeNeedsUpdate() {
        setNeedsStatusBarAppearanceUpdate()
        setNeedsUpdateOfHomeIndicatorAutoHidden()
    }

    private func applyNativeContentScale(to view: UIView) {
        let screen = view.window?.screen ?? UIScreen.main
        let scale = max(screen.nativeScale, screen.scale, 1.0)
        view.contentScaleFactor = scale
        view.layer.contentsScale = scale
        for subview in view.subviews {
            applyNativeContentScale(to: subview)
        }
    }
}


@objc public class SwiftUIHost: NSObject {
    @MainActor
    @objc public static func createMenuController() -> UIViewController {
        let hostingController = ARMSX2HostingController(rootView: RootView())
        hostingController.view.backgroundColor = .clear
        hostingController.view.isOpaque = false
        return hostingController
    }

    // Device haptic fallback for game rumble. Called from ARMSX2Bridge on the
    // main queue when no rumble-capable controller is connected.
    @MainActor
    @objc public static func triggerDeviceHaptic(large: UInt, small: UInt) {
        GameEventHaptics.shared.trigger(
            large: UInt16(truncatingIfNeeded: large),
            small: UInt16(truncatingIfNeeded: small)
        )
    }
}
