// PressSurface.swift — visual-only alpha-mask press feedback and UIKit press surface
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit

// MARK: - Visual-only alpha-mask press feedback
struct ARMSX2SkinMaskPressEffect: View {
    let button: ARMSX2PadButton
    let skin: VirtualPadSkin
    let descriptor: VPadSkinDescriptor
    let color: Color
    let isPressed: Bool
    let opacity: Double

    private var maskImage: UIImage? {
        let fileName = ControllerAsset.fileName(for: button)
        if let image = ControllerAsset.image(named: fileName, descriptor: descriptor) {
            return image
        }
        if skin != .legacyRefresh,
           let fallback = ControllerAsset.image(named: fileName, skin: .legacyRefresh) {
            return fallback
        }
        return nil
    }

    var body: some View {
        if isPressed, let image = maskImage {
            Image(uiImage: image)
                .resizable()
                .renderingMode(.template)
                .interpolation(.high)
                .antialiased(true)
                .scaledToFit()
                .foregroundStyle(color.opacity(0.34 * opacity))
                .overlay {
                    Image(uiImage: image)
                        .resizable()
                        .renderingMode(.template)
                        .interpolation(.high)
                        .antialiased(true)
                        .scaledToFit()
                        .foregroundStyle(color.opacity(0.42 * opacity))
                        .blendMode(.plusLighter)
                }
                .shadow(color: color.opacity(0.42 * opacity), radius: 9)
                .scaleEffect(0.92)
                .allowsHitTesting(false)
                .animation(.easeOut(duration: 0.06), value: isPressed)
        }
    }
}

@MainActor
enum ARMSX2VirtualPadMaskImageCache {
    private static var cachedImages: [String: UIImage] = [:]
    private static let buttons: [ARMSX2PadButton] = [
        .up, .down, .left, .right,
        .cross, .circle, .square, .triangle,
        .L1, .R1, .L2, .R2,
        .start, .select, .L3, .R3
    ]

    private static func key(button: ARMSX2PadButton, descriptor: VPadSkinDescriptor) -> String {
        let skinKey = descriptor.id
        return "\(skinKey):\(ControllerAsset.fileName(for: button))"
    }

    static func image(for button: ARMSX2PadButton, descriptor: VPadSkinDescriptor) -> UIImage? {
        let cacheKey = key(button: button, descriptor: descriptor)
        if let cached = cachedImages[cacheKey] {
            return cached
        }

        let fileName = ControllerAsset.fileName(for: button)
        let image = ControllerAsset.image(named: fileName, descriptor: descriptor)
            ?? ControllerAsset.image(named: fileName, skin: .legacyRefresh)
        guard let image else {
            return nil
        }

        // preparingForDisplay decodes/prepares the bitmap outside the first real press path when prewarmed.
        let prepared = image.preparingForDisplay() ?? image
        cachedImages[cacheKey] = prepared
        return prepared
    }

    static func prewarm(descriptor: VPadSkinDescriptor) {
        for button in buttons {
            _ = image(for: button, descriptor: descriptor)
        }
    }

    static func releaseForEmulationOnlyMode() {
        cachedImages.removeAll(keepingCapacity: false)
    }
}

enum VirtualPadPressSurfacePolicy {
    static func usesUIKitPressSurface(osMajorVersion: Int = ProcessInfo.processInfo.operatingSystemVersion.majorVersion) -> Bool {
        osMajorVersion >= 27
    }
}

func ARMSX2UsesUIKitPadPressSurface() -> Bool {
    VirtualPadPressSurfacePolicy.usesUIKitPressSurface()
}

@MainActor
struct UIKitPadPressSurface<Content: View>: UIViewRepresentable {
    let content: Content
    let onPress: (Bool) -> Void

    init(
        onPress: @escaping (Bool) -> Void,
        @ViewBuilder content: () -> Content
    ) {
        self.content = content()
        self.onPress = onPress
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(content: content, onPress: onPress)
    }

    func makeUIView(context: Context) -> UIButton {
        let button = UIButton(type: .custom)
        button.backgroundColor = .clear
        button.isExclusiveTouch = false

        button.addTarget(context.coordinator, action: #selector(Coordinator.touchDown), for: [.touchDown, .touchDragEnter])
        button.addTarget(context.coordinator, action: #selector(Coordinator.touchUp), for: [.touchUpInside, .touchUpOutside, .touchCancel, .touchDragExit])

        let hostedView = context.coordinator.hostingController.view!
        hostedView.backgroundColor = .clear
        hostedView.isUserInteractionEnabled = false
        hostedView.translatesAutoresizingMaskIntoConstraints = false
        button.addSubview(hostedView)
        NSLayoutConstraint.activate([
            hostedView.topAnchor.constraint(equalTo: button.topAnchor),
            hostedView.bottomAnchor.constraint(equalTo: button.bottomAnchor),
            hostedView.leadingAnchor.constraint(equalTo: button.leadingAnchor),
            hostedView.trailingAnchor.constraint(equalTo: button.trailingAnchor)
        ])

        return button
    }

    func updateUIView(_ uiView: UIButton, context: Context) {
        context.coordinator.onPress = onPress
        context.coordinator.hostingController.rootView = content
    }

    // Release the button's target/action pairs and detach the hosted view when SwiftUI
    // removes this representable (e.g. a control hidden through a visibility edit). Without
    // this, the retained UIHostingController and stale UIControl targets survive the view
    // removal and can corrupt touch dispatch after the pad is rebuilt.
    static func dismantleView(_ uiView: UIButton, coordinator: Coordinator) {
        uiView.removeTarget(coordinator, action: nil, for: .allEvents)
        coordinator.hostingController.willMove(toParent: nil)
        coordinator.hostingController.view?.removeFromSuperview()
        coordinator.hostingController.removeFromParent()
        coordinator.releasePress()
    }

    @MainActor
    final class Coordinator: NSObject {
        let hostingController: UIHostingController<Content>
        var onPress: (Bool) -> Void
        private var isPressed = false

        init(content: Content, onPress: @escaping (Bool) -> Void) {
            self.hostingController = UIHostingController(rootView: content)
            self.onPress = onPress
        }

        @objc func touchDown() {
            guard !isPressed else {
                return
            }

            isPressed = true
            onPress(true)
        }

        @objc func touchUp() {
            releasePress()
        }

        func releasePress() {
            guard isPressed else {
                return
            }

            isPressed = false
            onPress(false)
        }
    }
}
