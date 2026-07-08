// CompositeControls.swift — composite 8-way D-pad and combo face-button cluster
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit

// MARK: - Composite D-pad (8-way diagonals)
struct CompositeDPadFaceInfo {
    let button: ARMSX2PadButton
    let label: String
    let center: CGPoint
    let baseSize: CGFloat
    let visibleScaleX: CGFloat
    let visibleScaleY: CGFloat
    let hitScaleX: CGFloat
    let hitScaleY: CGFloat
}

// Resolves a composite D-pad touch against each arrow's actual hitbox. A touch in
// one arrow resolves to that direction; the overlap of two neighbours resolves to
// both (an intentional diagonal); the central deadzone stays neutral. A touch in the
// gap between arrows resolves to the nearest arrow by center distance.
private func compositeDPadResolve(
    offset: CGPoint,
    centroid: CGPoint,
    deadzone: CGFloat,
    faces: [CompositeDPadFaceInfo]
) -> Set<ARMSX2PadButton> {
    let distance = hypot(offset.x, offset.y)
    if distance < deadzone {
        return []
    }

    let point = CGPoint(x: centroid.x + offset.x, y: centroid.y + offset.y)

    var hits: [(face: CompositeDPadFaceInfo, distance: CGFloat)] = []
    for face in faces {
        let halfW = PadLayoutMetrics.touchLength(baseLength: face.baseSize, hitScale: face.hitScaleX) / 2
        let halfH = PadLayoutMetrics.touchLength(baseLength: face.baseSize, hitScale: face.hitScaleY) / 2
        let dx = abs(point.x - face.center.x)
        let dy = abs(point.y - face.center.y)
        if dx <= halfW && dy <= halfH {
            hits.append((face, hypot(dx, dy)))
        }
    }

    if hits.isEmpty {
        // Gap between arrows: resolve to the nearest arrow by center distance so a D-pad
        // press always registers a direction once past the deadzone, keeping slides fluid.
        guard let nearest = faces.min(by: {
            hypot($0.center.x - point.x, $0.center.y - point.y) <
            hypot($1.center.x - point.x, $1.center.y - point.y)
        }) else { return [] }
        return [nearest.button]
    }
    if hits.count == 1 {
        return [hits[0].face.button]
    }

    // Two or more hitboxes overlap here: press the two nearest neighbours (a diagonal),
    // never a broad chord.
    hits.sort { $0.distance < $1.distance }
    return Set(hits.prefix(2).map(\.face.button))
}

// Resolves a face-button touch against each button's own hitbox. A touch in a single
// hitbox resolves to that button; the overlap of two resolves to both (an
// intentional combo); the central gap resolves to nothing.
private func compositeFaceResolve(
    offset: CGPoint,
    centroid: CGPoint,
    faces: [CompositeFaceButtonInfo]
) -> Set<ARMSX2PadButton> {
    let point = CGPoint(x: centroid.x + offset.x, y: centroid.y + offset.y)

    var hits: [(face: CompositeFaceButtonInfo, distance: CGFloat)] = []
    for face in faces {
        let halfW = PadLayoutMetrics.touchLength(baseLength: face.baseSize, hitScale: face.hitScaleX) / 2
        let halfH = PadLayoutMetrics.touchLength(baseLength: face.baseSize, hitScale: face.hitScaleY) / 2
        let dx = abs(point.x - face.center.x)
        let dy = abs(point.y - face.center.y)
        if dx <= halfW && dy <= halfH {
            hits.append((face, hypot(dx, dy)))
        }
    }

    if hits.isEmpty { return [] }
    if hits.count == 1 { return [hits[0].face.button] }

    // Two or more hitboxes overlap here: press the two nearest neighbours, never a
    // broad chord.
    hits.sort { $0.distance < $1.distance }
    return Set(hits.prefix(2).map(\.face.button))
}

@MainActor
private protocol CompositeDPadTouchHandling: AnyObject {
    func touchBegan(at offset: CGPoint)
    func touchMoved(at offset: CGPoint)
    func touchEnded()
}

@MainActor
private final class CompositeDPadTouchView: UIView {
    weak var handler: CompositeDPadTouchHandling?

    override init(frame: CGRect) {
        super.init(frame: frame)
        configure()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        configure()
    }

    private func configure() {
        backgroundColor = .clear
        isExclusiveTouch = false
        isMultipleTouchEnabled = false
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let offset = offset(from: touches) else { return }
        handler?.touchBegan(at: offset)
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let offset = offset(from: touches) else { return }
        handler?.touchMoved(at: offset)
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        handler?.touchEnded()
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        handler?.touchEnded()
    }

    // Confine capture to the inscribed circle so the square frame's corners
    // never steal touches meant for neighbouring controls.
    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        let radius = min(bounds.width, bounds.height) / 2
        let dx = point.x - bounds.midX
        let dy = point.y - bounds.midY
        return (dx * dx) + (dy * dy) <= (radius * radius)
    }

    private func offset(from touches: Set<UITouch>) -> CGPoint? {
        guard let touch = touches.first else {
            return nil
        }
        let location = touch.location(in: self)
        return CGPoint(x: location.x - bounds.midX, y: location.y - bounds.midY)
    }
}

@MainActor
private struct CompositeDPadTouchSurface: UIViewRepresentable {
    let onBegan: () -> Void
    let onLocation: (CGPoint) -> Void
    let onEnded: () -> Void

    func makeCoordinator() -> Coordinator {
        Coordinator(onBegan: onBegan, onLocation: onLocation, onEnded: onEnded)
    }

    func makeUIView(context: Context) -> CompositeDPadTouchView {
        let view = CompositeDPadTouchView(frame: .zero)
        view.handler = context.coordinator
        return view
    }

    func updateUIView(_ uiView: CompositeDPadTouchView, context: Context) {
        context.coordinator.onBegan = onBegan
        context.coordinator.onLocation = onLocation
        context.coordinator.onEnded = onEnded
    }

    @MainActor
    final class Coordinator: NSObject, CompositeDPadTouchHandling {
        var onBegan: () -> Void
        var onLocation: (CGPoint) -> Void
        var onEnded: () -> Void

        init(onBegan: @escaping () -> Void, onLocation: @escaping (CGPoint) -> Void, onEnded: @escaping () -> Void) {
            self.onBegan = onBegan
            self.onLocation = onLocation
            self.onEnded = onEnded
        }

        func touchBegan(at offset: CGPoint) {
            onBegan()
            onLocation(offset)
        }

        func touchMoved(at offset: CGPoint) {
            onLocation(offset)
        }

        func touchEnded() {
            onEnded()
        }
    }
}

// Resolved per-direction art for one D-pad direction, ready to draw.
struct DirectionalFaceArt {
    let normal: UIImage?
    let pressed: UIImage?
}

struct CompositeDPadView: View {
    let faces: [CompositeDPadFaceInfo]
    let centroid: CGPoint
    let captureDiameter: CGFloat
    let deadzone: CGFloat
    var backgroundNormal: UIImage? = nil
    var backgroundPressed: UIImage? = nil
    var backgroundFrame: CGRect? = nil
    var directional: [ARMSX2PadButton: DirectionalFaceArt]? = nil

    @State private var pressed: Set<ARMSX2PadButton> = []
    @Environment(\.padOpacity) private var padOpacity
    @Environment(\.padUsesFullSkin) private var padUsesFullSkin

    var body: some View {
        ZStack {
            if let frame = backgroundFrame {
                let image = directional == nil
                    ? (pressed.isEmpty ? backgroundNormal : (backgroundPressed ?? backgroundNormal))
                    : backgroundNormal
                if let image {
                    Image(uiImage: image)
                        .resizable()
                        .interpolation(.high)
                        .antialiased(true)
                        .scaledToFit()
                        .frame(width: frame.width, height: frame.height)
                        .position(x: frame.midX, y: frame.midY)
                        .allowsHitTesting(false)
                }
            }

            if let directional {
                ForEach(faces, id: \.button) { face in
                    if let art = directional[face.button],
                       let image = pressed.contains(face.button) ? (art.pressed ?? art.normal) : art.normal {
                        Image(uiImage: image)
                            .resizable()
                            .interpolation(.high)
                            .antialiased(true)
                            .scaledToFit()
                            .frame(width: face.baseSize, height: face.baseSize)
                            .position(x: face.center.x, y: face.center.y)
                            .allowsHitTesting(false)
                    }
                }
            }
            CompositeDPadTouchSurface(
                onBegan: {
                    if SettingsStore.shared.hapticFeedback {
                        HapticManager.medium.impactOccurred()
                    }
                },
                onLocation: { offset in
                    apply(compositeDPadResolve(offset: offset, centroid: centroid, deadzone: deadzone, faces: faces))
                },
                onEnded: {
                    releaseAll()
                }
            )
            .frame(width: captureDiameter, height: captureDiameter)
            .position(x: centroid.x, y: centroid.y)

            ForEach(faces, id: \.button) { face in
                PadButtonFace(
                    button: face.button,
                    label: face.label,
                    baseW: face.baseSize,
                    baseH: face.baseSize,
                    visibleScaleX: face.visibleScaleX,
                    visibleScaleY: face.visibleScaleY,
                    hitScaleX: face.hitScaleX,
                    hitScaleY: face.hitScaleY,
                    isPressed: pressed.contains(face.button)
                )
                .allowsHitTesting(false)
                .opacity(padUsesFullSkin ? 1.0 : padOpacity)
                .position(x: face.center.x, y: face.center.y)
            }
        }
        .onDisappear {
            releaseAll()
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("D-pad")
        .accessibilityAddTraits(.isButton)
    }

    // Press only directions that newly entered the set and release only those
    // that left it, so quarter-circle slides never leave a stale direction held.
    private func apply(_ next: Set<ARMSX2PadButton>) {
        for button in pressed.subtracting(next) {
            EmulatorBridge.shared.setPadButton(button, pressed: false)
        }
        for button in next.subtracting(pressed) {
            EmulatorBridge.shared.setPadButton(button, pressed: true)
        }
        pressed = next
    }

    private func releaseAll() {
        let held = pressed
        guard !held.isEmpty else { return }
        for button in held {
            EmulatorBridge.shared.setPadButton(button, pressed: false)
        }
        pressed = []
    }
}

// MARK: - Composite face buttons (combo zones)
struct CompositeFaceButtonInfo {
    let button: ARMSX2PadButton
    let symbol: String
    let color: Color
    let center: CGPoint
    let baseSize: CGFloat
    let visibleScaleX: CGFloat
    let visibleScaleY: CGFloat
    let hitScaleX: CGFloat
    let hitScaleY: CGFloat
}

// Render-only visual for an action (face) button; the cluster owns all press/release.
private struct PSButtonFace: View {
    let button: ARMSX2PadButton
    let symbol: String
    let color: Color
    let baseSize: CGFloat
    var visibleScaleX: CGFloat = 1.0
    var visibleScaleY: CGFloat = 1.0
    var hitScaleX: CGFloat = 1.0
    var hitScaleY: CGFloat = 1.0
    let isPressed: Bool
    @Environment(\.padOpacity) private var padOpacity
    @Environment(\.padSkin) private var padSkin
    @Environment(\.padSkinDescriptor) private var padSkinDescriptor
    @Environment(\.padUsesFullSkin) private var padUsesFullSkin

    private var visibleW: CGFloat {
        PadLayoutMetrics.visibleLength(baseLength: baseSize, visibleScale: visibleScaleX)
    }

    private var visibleH: CGFloat {
        PadLayoutMetrics.visibleLength(baseLength: baseSize, visibleScale: visibleScaleY)
    }

    private var touchW: CGFloat {
        PadLayoutMetrics.touchLength(baseLength: baseSize, hitScale: hitScaleX)
    }

    private var touchH: CGFloat {
        PadLayoutMetrics.touchLength(baseLength: baseSize, hitScale: hitScaleY)
    }

    var body: some View {
        centeredButtonFace
    }

    private var centeredButtonFace: some View {
        ZStack(alignment: .topLeading) {
            Color.clear
                .frame(width: touchW, height: touchH)

            buttonFace
                .frame(width: visibleW, height: visibleH)
                .position(x: touchW / 2, y: touchH / 2)
        }
        .frame(width: touchW, height: touchH)
    }

    private var buttonFace: some View {
        ZStack {
            // Ellipse (not Circle) so the press-mask shape matches a non-uniform
            // visible frame; degenerates to a circle when the axes are equal.
            Ellipse()
                .fill(.clear)

            if !padUsesFullSkin || isPressed {
                ARMSX2SkinMaskPressEffect(button: button, skin: padSkin, descriptor: padSkinDescriptor, color: color, isPressed: isPressed, opacity: padUsesFullSkin ? padOpacity * 0.75 : padOpacity)
            }

            if !padUsesFullSkin {
                ControllerAssetImage(
                    fileName: ControllerAsset.fileName(for: button),
                    fallback: symbol,
                    fallbackColor: isPressed ? .white : color,
                    fallbackFontSize: min(visibleW, visibleH) * 0.42,
                    skin: padSkin,
                    descriptor: padSkinDescriptor
                )
                    .padding(padSkin == .crispVector ? 0 : max(1, min(visibleW, visibleH) * 0.03))
                    .brightness(isPressed ? 0.18 : 0)
                    .saturation(isPressed ? 1.16 : 1.0)
                    .scaleEffect(isPressed ? 0.90 : 1.0)
            }
        }
    }
}

@MainActor
private protocol CompositeFaceTouchHandling: AnyObject {
    func faceTouchesChanged(_ offsets: [CGPoint])
}

// Multi-touch surface for the face cluster: each touch is tracked independently by
// identity so independent two-finger presses work and a single finger can still
// slide into a two-button combo.
@MainActor
private final class CompositeFaceTouchView: UIView {
    weak var handler: CompositeFaceTouchHandling?
    private var tracked: [ObjectIdentifier: CGPoint] = [:]

    override init(frame: CGRect) {
        super.init(frame: frame)
        configure()
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        configure()
    }

    private func configure() {
        backgroundColor = .clear
        isExclusiveTouch = false
        isMultipleTouchEnabled = true
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        for touch in touches {
            tracked[ObjectIdentifier(touch)] = offset(of: touch)
        }
        handler?.faceTouchesChanged(Array(tracked.values))
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        for touch in touches {
            tracked[ObjectIdentifier(touch)] = offset(of: touch)
        }
        handler?.faceTouchesChanged(Array(tracked.values))
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        for touch in touches {
            tracked[ObjectIdentifier(touch)] = nil
        }
        handler?.faceTouchesChanged(Array(tracked.values))
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        for touch in touches {
            tracked[ObjectIdentifier(touch)] = nil
        }
        handler?.faceTouchesChanged(Array(tracked.values))
    }

    // Confine new touches to the inscribed circle so the square frame's corners
    // never steal touches meant for neighbouring controls.
    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        let radius = min(bounds.width, bounds.height) / 2
        let dx = point.x - bounds.midX
        let dy = point.y - bounds.midY
        return (dx * dx) + (dy * dy) <= (radius * radius)
    }

    private func offset(of touch: UITouch) -> CGPoint {
        let location = touch.location(in: self)
        return CGPoint(x: location.x - bounds.midX, y: location.y - bounds.midY)
    }

    // Release every button if the surface is torn down while touches are in flight
    // (e.g. orientation change or navigation). The SwiftUI view also calls releaseAll
    // on disappear; this is a belt-and-suspenders guard at the UIKit layer.
    override func removeFromSuperview() {
        handler?.faceTouchesChanged([])
        super.removeFromSuperview()
    }
}

@MainActor
private struct CompositeFaceTouchSurface: UIViewRepresentable {
    let onOffsets: ([CGPoint]) -> Void

    func makeCoordinator() -> Coordinator {
        Coordinator(onOffsets: onOffsets)
    }

    func makeUIView(context: Context) -> CompositeFaceTouchView {
        let view = CompositeFaceTouchView(frame: .zero)
        view.handler = context.coordinator
        return view
    }

    func updateUIView(_ uiView: CompositeFaceTouchView, context: Context) {
        context.coordinator.onOffsets = onOffsets
    }

    @MainActor
    final class Coordinator: NSObject, CompositeFaceTouchHandling {
        var onOffsets: ([CGPoint]) -> Void

        init(onOffsets: @escaping ([CGPoint]) -> Void) {
            self.onOffsets = onOffsets
        }

        func faceTouchesChanged(_ offsets: [CGPoint]) {
            onOffsets(offsets)
        }
    }
}

// Composite face-button cluster: one multi-touch surface over the four action
// buttons; the union of active touches is pressed. Buttons leaving the union
// release immediately so no press goes stale.
struct CompositeFaceView: View {
    let faces: [CompositeFaceButtonInfo]
    let centroid: CGPoint
    let captureDiameter: CGFloat

    @State private var pressed: Set<ARMSX2PadButton> = []
    @Environment(\.padOpacity) private var padOpacity
    @Environment(\.padUsesFullSkin) private var padUsesFullSkin

    var body: some View {
        ZStack {
            CompositeFaceTouchSurface { offsets in
                var next: Set<ARMSX2PadButton> = []
                for offset in offsets {
                    next.formUnion(compositeFaceResolve(
                        offset: offset,
                        centroid: centroid,
                        faces: faces
                    ))
                }
                apply(next)
            }
            .frame(width: captureDiameter, height: captureDiameter)
            .position(x: centroid.x, y: centroid.y)

            ForEach(faces, id: \.button) { face in
                PSButtonFace(
                    button: face.button,
                    symbol: face.symbol,
                    color: face.color,
                    baseSize: face.baseSize,
                    visibleScaleX: face.visibleScaleX,
                    visibleScaleY: face.visibleScaleY,
                    hitScaleX: face.hitScaleX,
                    hitScaleY: face.hitScaleY,
                    isPressed: pressed.contains(face.button)
                )
                .allowsHitTesting(false)
                .opacity(padUsesFullSkin ? 1.0 : padOpacity)
                .position(x: face.center.x, y: face.center.y)
            }
        }
        .onDisappear {
            releaseAll()
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("Face buttons")
        .accessibilityAddTraits(.isButton)
    }

    // Press only buttons that newly entered the union and release only those that
    // left it, so slides and multi-touch never leave a stale button held. Haptic
    // fires once per change that newly presses at least one button.
    private func apply(_ next: Set<ARMSX2PadButton>) {
        let released = pressed.subtracting(next)
        let newlyPressed = next.subtracting(pressed)
        for button in released {
            EmulatorBridge.shared.setPadButton(button, pressed: false)
        }
        for button in newlyPressed {
            EmulatorBridge.shared.setPadButton(button, pressed: true)
        }
        if !newlyPressed.isEmpty, SettingsStore.shared.hapticFeedback {
            HapticManager.medium.impactOccurred()
        }
        pressed = next
    }

    private func releaseAll() {
        let held = pressed
        guard !held.isEmpty else { return }
        for button in held {
            EmulatorBridge.shared.setPadButton(button, pressed: false)
        }
        pressed = []
    }
}
