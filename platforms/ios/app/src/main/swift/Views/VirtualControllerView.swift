// VirtualControllerView.swift — PS2 DualShock2 virtual controller
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit

// Singleton haptic generator — prepared once, reused for all button presses
@MainActor
enum HapticManager {
    static let medium: UIImpactFeedbackGenerator = {
        let g = UIImpactFeedbackGenerator(style: .medium)
        g.prepare()
        return g
    }()
    static let light: UIImpactFeedbackGenerator = {
        let g = UIImpactFeedbackGenerator(style: .light)
        g.prepare()
        return g
    }()
}

struct VirtualControllerView: View {
    @State private var settings = SettingsStore.shared
    @State private var skinLibrary = VPadSkinLibraryStore.shared
    @State private var layout = PadLayoutStore.shared
    var isLandscape: Bool = false
    var layoutSnapshot: PadLayoutSnapshot? = nil
    var skinDescriptor: VPadSkinDescriptor? = nil

    @State private var v2Layout: SkinManifestRuntimeLayout? = nil

    private var analogStickScale: CGFloat {
        min(max(CGFloat(settings.analogStickScale), 0.8), 1.6)
    }

    private var effectiveSkinDescriptor: VPadSkinDescriptor {
        skinDescriptor ?? skinLibrary.selectedDescriptor
    }

    // MARK: - Manifest v2 runtime rendering

    // Uniform transform from the manifest mapping size to the controller canvas.
    private struct V2CanvasTransform {
        let originX: CGFloat
        let originY: CGFloat
        let scaleX: CGFloat
        let scaleY: CGFloat

        func rect(_ frame: NormalizedFrame) -> CGRect {
            CGRect(
                x: originX + frame.x * scaleX,
                y: originY + frame.y * scaleY,
                width: frame.width * scaleX,
                height: frame.height * scaleY
            )
        }
    }

    private func v2Transform(mappingWidth: CGFloat, mappingHeight: CGFloat, canvasW: CGFloat, canvasH: CGFloat) -> V2CanvasTransform {
        let cover = max(canvasW / mappingWidth, canvasH / mappingHeight)
        let sx = mappingWidth * cover
        let sy = mappingHeight * cover
        return V2CanvasTransform(originX: (canvasW - sx) / 2, originY: (canvasH - sy) / 2, scaleX: sx, scaleY: sy)
    }

    private var v2AssetsDirectory: URL? {
        let descriptor = effectiveSkinDescriptor
        guard descriptor.source == .imported, descriptor.manifestVersion == 2 else { return nil }
        return skinLibrary.importedAssetsDirectory(for: descriptor)
    }

    private var v2CacheKey: String {
        "\(effectiveSkinDescriptor.id)|\(isLandscape ? "l" : "p")"
    }

    private func v2VisualRect(_ control: SkinManifestRuntimeLayout.Control, transform: V2CanvasTransform) -> CGRect {
        transform.rect(control.visualFrame)
    }

    private func v2HitRect(_ control: SkinManifestRuntimeLayout.Control, transform: V2CanvasTransform) -> CGRect {
        let visual = v2VisualRect(control, transform: transform)
        let insets = control.hitInsets
        return CGRect(
            x: visual.minX - insets.left * transform.scaleX,
            y: visual.minY - insets.top * transform.scaleY,
            width: visual.width + (insets.left + insets.right) * transform.scaleX,
            height: visual.height + (insets.top + insets.bottom) * transform.scaleY
        )
    }

    @ViewBuilder
    private func v2ControllerOverlay(layout: SkinManifestRuntimeLayout, assetsDirectory: URL, descriptor: VPadSkinDescriptor, w: CGFloat, h: CGFloat) -> some View {
        let transform = v2Transform(mappingWidth: layout.mappingWidth, mappingHeight: layout.mappingHeight, canvasW: w, canvasH: h)
        ZStack {
            if let backgroundPath = layout.backgroundAssetPath,
               let backgroundImage = SkinManifestRuntimeLayout.image(forRelativePath: backgroundPath, in: assetsDirectory) {
                Image(uiImage: backgroundImage)
                    .resizable()
                    .interpolation(.high)
                    .antialiased(true)
                    .scaledToFill()
                    .frame(width: transform.scaleX, height: transform.scaleY)
                    .clipped()
                    .allowsHitTesting(false)
            }

            ForEach(layout.controls) { control in
                v2ControlView(control, transform: transform, assetsDirectory: assetsDirectory, descriptor: descriptor)
            }

            if layout.debug {
                ForEach(layout.controls) { control in
                    let hit = v2HitRect(control, transform: transform)
                    Rectangle()
                        .stroke(.yellow.opacity(0.5), lineWidth: 1)
                        .frame(width: hit.width, height: hit.height)
                        .position(x: hit.midX, y: hit.midY)
                        .allowsHitTesting(false)
                }
            }
        }
        .frame(width: w, height: h)
        .clipped()
    }

    @ViewBuilder
    private func v2ControlView(_ control: SkinManifestRuntimeLayout.Control, transform: V2CanvasTransform, assetsDirectory: URL, descriptor: VPadSkinDescriptor) -> some View {
        let visual = v2VisualRect(control, transform: transform)
        let hit = v2HitRect(control, transform: transform)
        let knobSize: CGSize? = {
            if let kw = control.knobWidth, let kh = control.knobHeight {
                CGSize(width: kw * transform.scaleX, height: kh * transform.scaleY)
            } else {
                nil
            }
        }()
        switch control.placement {
        case .inert:
            EmptyView()
        case .button(let button):
            SkinManifestButtonView(
                button: button,
                visualRect: visual,
                hitRect: hit,
                normalPath: control.normalAssetPath,
                pressedPath: control.pressedAssetPath,
                assetsDirectory: assetsDirectory,
                descriptor: descriptor
            )
        case .dpad:
            v2DPadView(visualRect: visual, hitRect: hit, normalPath: control.normalAssetPath, pressedPath: control.pressedAssetPath, directional: control.directional, assetsDirectory: assetsDirectory)
        case .thumbstick(let side, _):
            v2StickView(visualRect: visual, captureDiameter: min(hit.width, hit.height), normalPath: control.normalAssetPath, knobPath: control.knobAssetPath, knobSize: knobSize, assetsDirectory: assetsDirectory, side: side)
        }
    }

    @ViewBuilder
    private func v2DPadView(visualRect: CGRect, hitRect: CGRect, normalPath: String?, pressedPath: String?, directional: SkinManifestRuntimeLayout.DirectionalAssetPaths?, assetsDirectory: URL) -> some View {
        let minDimension = min(visualRect.width, visualRect.height)
        let halfStep = minDimension * 0.29
        let arrowSize = minDimension * 0.42
        let centroid = CGPoint(x: visualRect.midX, y: visualRect.midY)
        let faces: [CompositeDPadFaceInfo] = [
            .init(button: .up, label: "\u{25b2}", center: CGPoint(x: centroid.x, y: centroid.y - halfStep), baseSize: arrowSize, visibleScaleX: 1, visibleScaleY: 1, hitScaleX: 1, hitScaleY: 1),
            .init(button: .down, label: "\u{25bc}", center: CGPoint(x: centroid.x, y: centroid.y + halfStep), baseSize: arrowSize, visibleScaleX: 1, visibleScaleY: 1, hitScaleX: 1, hitScaleY: 1),
            .init(button: .left, label: "\u{25c0}", center: CGPoint(x: centroid.x - halfStep, y: centroid.y), baseSize: arrowSize, visibleScaleX: 1, visibleScaleY: 1, hitScaleX: 1, hitScaleY: 1),
            .init(button: .right, label: "\u{25b6}", center: CGPoint(x: centroid.x + halfStep, y: centroid.y), baseSize: arrowSize, visibleScaleX: 1, visibleScaleY: 1, hitScaleX: 1, hitScaleY: 1)
        ]
        let captureDiameter = max(min(hitRect.width, hitRect.height), minDimension)
        let deadzone = minDimension * 0.12
        let normalImage = normalPath.flatMap { SkinManifestRuntimeLayout.image(forRelativePath: $0, in: assetsDirectory) }
        let pressedImage = pressedPath.flatMap { SkinManifestRuntimeLayout.image(forRelativePath: $0, in: assetsDirectory) }
        let directionalArt: [ARMSX2PadButton: DirectionalFaceArt]? = directional.map { dirs in
            func resolve(_ d: SkinManifestRuntimeLayout.DirectionalAssetPath) -> DirectionalFaceArt {
                DirectionalFaceArt(
                    normal: d.normalPath.flatMap { SkinManifestRuntimeLayout.image(forRelativePath: $0, in: assetsDirectory) },
                    pressed: d.pressedPath.flatMap { SkinManifestRuntimeLayout.image(forRelativePath: $0, in: assetsDirectory) }
                )
            }
            return [
                .up: resolve(dirs.up),
                .down: resolve(dirs.down),
                .left: resolve(dirs.left),
                .right: resolve(dirs.right)
            ]
        }
        CompositeDPadView(
            faces: faces,
            centroid: centroid,
            captureDiameter: captureDiameter,
            deadzone: deadzone,
            backgroundNormal: normalImage,
            backgroundPressed: pressedImage,
            backgroundFrame: visualRect,
            directional: directionalArt
        )
        .environment(\.padUsesFullSkin, true)
    }

    @ViewBuilder
    private func v2StickView(visualRect: CGRect, captureDiameter: CGFloat, normalPath: String?, knobPath: String?, knobSize: CGSize?, assetsDirectory: URL, side: SkinManifestRuntimeLayout.Side) -> some View {
        let target = min(visualRect.width, visualRect.height)
        let sizeScale = min(max(target / 68.0, 0.8), 1.6)
        let knobImage = knobPath.flatMap({ SkinManifestRuntimeLayout.image(forRelativePath: $0, in: assetsDirectory) })
        ZStack {
            if let baseImage = normalPath.flatMap({ SkinManifestRuntimeLayout.image(forRelativePath: $0, in: assetsDirectory) }) {
                Image(uiImage: baseImage)
                    .resizable()
                    .interpolation(.high)
                    .antialiased(true)
                    .scaledToFit()
                    .frame(width: visualRect.width, height: visualRect.height)
                    .allowsHitTesting(false)
            }
            StickView(isLeft: side == .left, sizeScale: sizeScale, layoutScale: 1.0, captureDiameter: captureDiameter, knobImage: knobImage, knobSize: knobSize)
        }
        .position(x: visualRect.midX, y: visualRect.midY)
    }

    var body: some View {
        GeometryReader { geo in
            let descriptor = effectiveSkinDescriptor
            let skin = descriptor.virtualPadSkin
            let usesFullSkin = ControllerAsset.gameplayFullSkinImage(descriptor: descriptor, isLandscape: isLandscape) != nil
            let v2Assets = v2AssetsDirectory

            if isLandscape {
                Group {
                    if let v2 = v2Layout, let assets = v2Assets {
                        v2ControllerOverlay(layout: v2, assetsDirectory: assets, descriptor: descriptor, w: geo.size.width, h: geo.size.height)
                    } else {
                        landscapeLayout(w: geo.size.width, h: geo.size.height)
                    }
                }
                .environment(\.padOpacity, Double(settings.padOpacity))
                .environment(\.padSkin, skin)
                .environment(\.padSkinDescriptor, descriptor)
                .environment(\.padUsesFullSkin, usesFullSkin)
            } else {
                Group {
                    if let v2 = v2Layout, let assets = v2Assets {
                        v2ControllerOverlay(layout: v2, assetsDirectory: assets, descriptor: descriptor, w: geo.size.width, h: geo.size.height)
                    } else {
                        portraitLayout(w: geo.size.width, h: geo.size.height)
                    }
                }
                .environment(\.padOpacity, Double(settings.padOpacity))
                .environment(\.padSkin, skin)
                .environment(\.padSkinDescriptor, descriptor)
                .environment(\.padUsesFullSkin, usesFullSkin)
            }
        }
        // Prepare mask images before gameplay input so the first press cannot decode/scan on the hot path.
        .onAppear {
            ARMSX2VirtualPadMaskImageCache.prewarm(descriptor: effectiveSkinDescriptor)
        }
        .onChange(of: skinLibrary.selectedSkinID) { _, _ in
            ARMSX2VirtualPadMaskImageCache.prewarm(descriptor: effectiveSkinDescriptor)
        }
        .onChange(of: skinDescriptor) { _, _ in
            ARMSX2VirtualPadMaskImageCache.prewarm(descriptor: effectiveSkinDescriptor)
        }
        .task(id: v2CacheKey) {
            v2Layout = SkinManifestRuntimeLayout.make(
                for: effectiveSkinDescriptor,
                isLandscape: isLandscape,
                device: SkinManifestRuntimeLayout.currentDevice(),
                screenClass: SkinManifestRuntimeLayout.currentScreenClass()
            )
        }

    }

    private func pos(_ id: String, landscape: Bool) -> PadGroupPosition {
        layoutSnapshot?.position(for: id, landscape: landscape) ?? layout.position(for: id, landscape: landscape)
    }

    private func perButtonPos(_ id: String, landscape: Bool, w: CGFloat, h: CGFloat) -> PadGroupPosition {
        layoutSnapshot?.perButtonPosition(for: id, landscape: landscape, areaW: w, areaH: h)
            ?? layout.perButtonPosition(for: id, landscape: landscape, areaW: w, areaH: h)
    }

    private func isVisible(_ id: String) -> Bool {
        layoutSnapshot?.isControlVisible(id) ?? layout.isControlVisible(id)
    }

    @ViewBuilder
    private func placedPadButton(
        id: String,
        label: String,
        w: CGFloat,
        h: CGFloat,
        btn: ARMSX2PadButton,
        landscape: Bool,
        areaW: CGFloat,
        areaH: CGFloat,
        perButton: Bool = false
    ) -> some View {
        let p = perButton ? perButtonPos(id, landscape: landscape, w: areaW, h: areaH) : pos(id, landscape: landscape)
        PadBtn(label: label, w: w, h: h, btn: btn, visibleScaleX: p.scaleX, visibleScaleY: p.scaleY, hitScaleX: p.hitScaleX, hitScaleY: p.hitScaleY)
            .position(x: p.x * areaW, y: p.y * areaH)
    }

    // Composite 8-way D-pad used when D-pad diagonals are enabled. Derives the
    // capture circle and center deadzone from the four cardinal button positions
    // so it follows custom layouts, per-orientation sizing, and hit scaling.
    private func placedCompositeDPad(landscape: Bool, areaW: CGFloat, areaH: CGFloat) -> some View {
        let entries: [(id: String, button: ARMSX2PadButton, label: String)] = [
            ("up", .up, "▲"), ("down", .down, "▼"), ("left", .left, "◀"), ("right", .right, "▶")
        ]
        let dpadW = VirtualPadButtonOffset.dpadButtonWidth(isLandscape: landscape)

        var faces: [CompositeDPadFaceInfo] = []
        var centers: [CGPoint] = []
        for entry in entries {
            let p = perButtonPos(entry.id, landscape: landscape, w: areaW, h: areaH)
            let center = CGPoint(x: p.x * areaW, y: p.y * areaH)
            centers.append(center)
            faces.append(CompositeDPadFaceInfo(
                button: entry.button,
                label: entry.label,
                center: center,
                baseSize: dpadW,
                visibleScaleX: p.scaleX,
                visibleScaleY: p.scaleY,
                hitScaleX: p.hitScaleX,
                hitScaleY: p.hitScaleY
            ))
        }

        let centroid = CGPoint(
            x: centers.reduce(0, { $0 + $1.x }) / CGFloat(centers.count),
            y: centers.reduce(0, { $0 + $1.y }) / CGFloat(centers.count)
        )
        let distances = centers.map { hypot($0.x - centroid.x, $0.y - centroid.y) }
        let maxTouchHalf = faces.map { PadLayoutMetrics.touchLength(baseLength: dpadW, hitScale: $0.hitScaleX) / 2 }.max() ?? 0
        let captureRadius = (distances.max() ?? 0) + maxTouchHalf
        let nearestDistance = distances.min() ?? captureRadius
        let deadzone = nearestDistance * 0.20

        return CompositeDPadView(
            faces: faces,
            centroid: centroid,
            captureDiameter: captureRadius * 2,
            deadzone: deadzone
        )
    }

    // Composite face-button cluster used when face-button combo zones are enabled.
    // Derives its centroid and capture circle from the four action-button positions
    // so it follows custom layouts, per-axis sizing, and hit scaling.
    private func placedCompositeFaceButtons(landscape: Bool, areaW: CGFloat, areaH: CGFloat) -> some View {
        let entries: [(id: String, button: ARMSX2PadButton, sym: String, clr: Color)] = [
            ("triangle", .triangle, "△", .green),
            ("cross", .cross, "✕", .blue),
            ("square", .square, "□", .pink),
            ("circle", .circle, "○", .red)
        ]
        let actionSz = VirtualPadButtonOffset.actionButtonSize

        var faces: [CompositeFaceButtonInfo] = []
        var centers: [CGPoint] = []
        for entry in entries {
            let p = perButtonPos(entry.id, landscape: landscape, w: areaW, h: areaH)
            let center = CGPoint(x: p.x * areaW, y: p.y * areaH)
            centers.append(center)
            faces.append(CompositeFaceButtonInfo(
                button: entry.button,
                symbol: entry.sym,
                color: entry.clr,
                center: center,
                baseSize: actionSz,
                visibleScaleX: p.scaleX,
                visibleScaleY: p.scaleY,
                hitScaleX: p.hitScaleX,
                hitScaleY: p.hitScaleY
            ))
        }

        let centroid = CGPoint(
            x: centers.reduce(0, { $0 + $1.x }) / CGFloat(centers.count),
            y: centers.reduce(0, { $0 + $1.y }) / CGFloat(centers.count)
        )
        let captureRadius = faces.map { face in
            let centerDistance = hypot(face.center.x - centroid.x, face.center.y - centroid.y)
            let halfW = PadLayoutMetrics.touchLength(baseLength: face.baseSize, hitScale: face.hitScaleX) / 2
            let halfH = PadLayoutMetrics.touchLength(baseLength: face.baseSize, hitScale: face.hitScaleY) / 2
            return centerDistance + hypot(halfW, halfH)
        }.max() ?? 0

        return CompositeFaceView(
            faces: faces,
            centroid: centroid,
            captureDiameter: captureRadius * 2
        )
    }

    @ViewBuilder
    private func placedPSButton(
        id: String,
        sym: String,
        clr: Color,
        sz: CGFloat,
        btn: ARMSX2PadButton,
        landscape: Bool,
        areaW: CGFloat,
        areaH: CGFloat
    ) -> some View {
        let p = perButtonPos(id, landscape: landscape, w: areaW, h: areaH)
        PSBtn(sym: sym, clr: clr, sz: sz, btn: btn, visibleScaleX: p.scaleX, visibleScaleY: p.scaleY, hitScaleX: p.hitScaleX, hitScaleY: p.hitScaleY)
            .position(x: p.x * areaW, y: p.y * areaH)
    }

    @ViewBuilder
    private func placedStick(
        id: String,
        isLeft: Bool,
        landscape: Bool,
        areaW: CGFloat,
        areaH: CGFloat
    ) -> some View {
        let p = pos(id, landscape: landscape)
        StickView(isLeft: isLeft, sizeScale: analogStickScale, layoutScale: p.scale)
            .position(x: p.x * areaW, y: p.y * areaH)
    }

    // MARK: - Landscape: overlay on game screen
    @ViewBuilder
    func landscapeLayout(w: CGFloat, h: CGFloat) -> some View {
        ZStack {
            if let fullSkin = ControllerAsset.gameplayFullSkinImage(descriptor: effectiveSkinDescriptor, isLandscape: true) {
                Image(uiImage: fullSkin)
                    .resizable()
                    .interpolation(.high)
                    .antialiased(true)
                    .scaledToFill()
                    .frame(width: w, height: h)
                    .clipped()
                    .allowsHitTesting(false)
            }

            // D-pad buttons
            if isVisible("dpad") {
                if settings.dpadDiagonalsEnabled {
                    placedCompositeDPad(landscape: true, areaW: w, areaH: h)
                } else {
                    let dpadW = VirtualPadButtonOffset.dpadButtonWidth(isLandscape: true)
                    placedPadButton(id: "up", label: "▲", w: dpadW, h: dpadW, btn: .up, landscape: true, areaW: w, areaH: h, perButton: true)
                    placedPadButton(id: "down", label: "▼", w: dpadW, h: dpadW, btn: .down, landscape: true, areaW: w, areaH: h, perButton: true)
                    placedPadButton(id: "left", label: "◀", w: dpadW, h: dpadW, btn: .left, landscape: true, areaW: w, areaH: h, perButton: true)
                    placedPadButton(id: "right", label: "▶", w: dpadW, h: dpadW, btn: .right, landscape: true, areaW: w, areaH: h, perButton: true)
                }
            }

            // Action buttons: composite combo surface when enabled (and all four face
            // buttons are visible); otherwise individual buttons with per-button visibility.
            if settings.faceComboZonesEnabled,
               isVisible("triangle") && isVisible("cross") && isVisible("square") && isVisible("circle") {
                placedCompositeFaceButtons(landscape: true, areaW: w, areaH: h)
            } else {
                let actionSz = VirtualPadButtonOffset.actionButtonSize
                if isVisible("triangle") {
                    placedPSButton(id: "triangle", sym: "△", clr: .green, sz: actionSz, btn: .triangle, landscape: true, areaW: w, areaH: h)
                }
                if isVisible("cross") {
                    placedPSButton(id: "cross", sym: "✕", clr: .blue, sz: actionSz, btn: .cross, landscape: true, areaW: w, areaH: h)
                }
                if isVisible("square") {
                    placedPSButton(id: "square", sym: "□", clr: .pink, sz: actionSz, btn: .square, landscape: true, areaW: w, areaH: h)
                }
                if isVisible("circle") {
                    placedPSButton(id: "circle", sym: "○", clr: .red, sz: actionSz, btn: .circle, landscape: true, areaW: w, areaH: h)
                }
            }

            if isVisible("l2") {
                placedPadButton(id: "l2", label: "L2", w: 130, h: 44, btn: .L2, landscape: true, areaW: w, areaH: h)
            }
            if isVisible("l1") {
                placedPadButton(id: "l1", label: "L1", w: 120, h: 32, btn: .L1, landscape: true, areaW: w, areaH: h)
            }
            if isVisible("r2") {
                placedPadButton(id: "r2", label: "R2", w: 130, h: 44, btn: .R2, landscape: true, areaW: w, areaH: h)
            }
            if isVisible("r1") {
                placedPadButton(id: "r1", label: "R1", w: 120, h: 32, btn: .R1, landscape: true, areaW: w, areaH: h)
            }
            if isVisible("select") {
                placedPadButton(id: "select", label: "SEL", w: 40, h: 22, btn: .select, landscape: true, areaW: w, areaH: h)
            }
            if isVisible("start") {
                placedPadButton(id: "start", label: "START", w: 48, h: 22, btn: .start, landscape: true, areaW: w, areaH: h)
            }
            if isVisible("lstick") {
                placedStick(id: "lstick", isLeft: true, landscape: true, areaW: w, areaH: h)
            }
            if isVisible("rstick") {
                placedStick(id: "rstick", isLeft: false, landscape: true, areaW: w, areaH: h)
            }
        }
    }

    // MARK: - Portrait: controller fills its given area
    @ViewBuilder
    func portraitLayout(w: CGFloat, h: CGFloat) -> some View {
        ZStack {
            if let fullSkin = ControllerAsset.gameplayFullSkinImage(descriptor: effectiveSkinDescriptor, isLandscape: false) {
                Image(uiImage: fullSkin)
                    .resizable()
                    .interpolation(.high)
                    .antialiased(true)
                    .scaledToFill()
                    .frame(width: w, height: h)
                    .clipped()
                    .allowsHitTesting(false)
            }

            GeometryReader { cGeo in
                let cW = cGeo.size.width
                let cH = cGeo.size.height

                if isVisible("l2") {
                    placedPadButton(id: "l2", label: "L2", w: 110, h: 40, btn: .L2, landscape: false, areaW: cW, areaH: cH)
                }
                if isVisible("l1") {
                    placedPadButton(id: "l1", label: "L1", w: 100, h: 30, btn: .L1, landscape: false, areaW: cW, areaH: cH)
                }
                if isVisible("r2") {
                    placedPadButton(id: "r2", label: "R2", w: 110, h: 40, btn: .R2, landscape: false, areaW: cW, areaH: cH)
                }
                if isVisible("r1") {
                    placedPadButton(id: "r1", label: "R1", w: 100, h: 30, btn: .R1, landscape: false, areaW: cW, areaH: cH)
                }
                if isVisible("select") {
                    placedPadButton(id: "select", label: "SEL", w: 42, h: 22, btn: .select, landscape: false, areaW: cW, areaH: cH)
                }
                if isVisible("start") {
                    placedPadButton(id: "start", label: "START", w: 48, h: 22, btn: .start, landscape: false, areaW: cW, areaH: cH)
                }

                // D-pad buttons
                if isVisible("dpad") {
                    if settings.dpadDiagonalsEnabled {
                        placedCompositeDPad(landscape: false, areaW: cW, areaH: cH)
                    } else {
                        let dpadW = VirtualPadButtonOffset.dpadButtonWidth(isLandscape: false)
                        placedPadButton(id: "up", label: "▲", w: dpadW, h: dpadW, btn: .up, landscape: false, areaW: cW, areaH: cH, perButton: true)
                        placedPadButton(id: "down", label: "▼", w: dpadW, h: dpadW, btn: .down, landscape: false, areaW: cW, areaH: cH, perButton: true)
                        placedPadButton(id: "left", label: "◀", w: dpadW, h: dpadW, btn: .left, landscape: false, areaW: cW, areaH: cH, perButton: true)
                        placedPadButton(id: "right", label: "▶", w: dpadW, h: dpadW, btn: .right, landscape: false, areaW: cW, areaH: cH, perButton: true)
                    }
                }

                // Action buttons: composite combo surface when enabled (and all four face
                // buttons are visible); otherwise individual buttons with per-button visibility.
                if settings.faceComboZonesEnabled,
                   isVisible("triangle") && isVisible("cross") && isVisible("square") && isVisible("circle") {
                    placedCompositeFaceButtons(landscape: false, areaW: cW, areaH: cH)
                } else {
                    let actionSz = VirtualPadButtonOffset.actionButtonSize
                    if isVisible("triangle") {
                        placedPSButton(id: "triangle", sym: "△", clr: .green, sz: actionSz, btn: .triangle, landscape: false, areaW: cW, areaH: cH)
                    }
                    if isVisible("cross") {
                        placedPSButton(id: "cross", sym: "✕", clr: .blue, sz: actionSz, btn: .cross, landscape: false, areaW: cW, areaH: cH)
                    }
                    if isVisible("square") {
                        placedPSButton(id: "square", sym: "□", clr: .pink, sz: actionSz, btn: .square, landscape: false, areaW: cW, areaH: cH)
                    }
                    if isVisible("circle") {
                        placedPSButton(id: "circle", sym: "○", clr: .red, sz: actionSz, btn: .circle, landscape: false, areaW: cW, areaH: cH)
                    }
                }

                if isVisible("lstick") {
                    placedStick(id: "lstick", isLeft: true, landscape: false, areaW: cW, areaH: cH)
                }
                if isVisible("rstick") {
                    placedStick(id: "rstick", isLeft: false, landscape: false, areaW: cW, areaH: cH)
                }
            }
        }
    }
}

// MARK: - Manifest v2 single button
private struct SkinManifestButtonView: View {
    let button: ARMSX2PadButton
    let visualRect: CGRect
    let hitRect: CGRect
    let normalPath: String?
    let pressedPath: String?
    let assetsDirectory: URL
    let descriptor: VPadSkinDescriptor
    @State private var normalImage: UIImage? = nil
    @State private var pressedImage: UIImage? = nil
    @State private var on = false
    @Environment(\.padOpacity) private var padOpacity

    private var symbol: String {
        switch button {
        case .up: return "\u{25b2}"
        case .down: return "\u{25bc}"
        case .left: return "\u{25c0}"
        case .right: return "\u{25b6}"
        case .cross: return "\u{2715}"
        case .circle: return "\u{25cb}"
        case .square: return "\u{25a1}"
        case .triangle: return "\u{25b3}"
        case .L1: return "L1"
        case .L2: return "L2"
        case .R1: return "R1"
        case .R2: return "R2"
        case .start: return "START"
        case .select: return "SEL"
        case .L3: return "L3"
        case .R3: return "R3"
        @unknown default: return ""
        }
    }

    private var accessibilityName: String {
        switch button {
        case .up: return "D-pad up"
        case .down: return "D-pad down"
        case .left: return "D-pad left"
        case .right: return "D-pad right"
        case .triangle: return "Triangle"
        case .cross: return "Cross"
        case .square: return "Square"
        case .circle: return "Circle"
        case .L1: return "L1"
        case .R1: return "R1"
        case .L2: return "L2"
        case .R2: return "R2"
        case .start: return "Start"
        case .select: return "Select"
        case .L3: return "L3"
        case .R3: return "R3"
        @unknown default: return symbol
        }
    }

    var body: some View {
        ZStack {
            Color.clear
                .frame(width: hitRect.width, height: hitRect.height)
                .contentShape(Rectangle())
                .gesture(
                    DragGesture(minimumDistance: 0)
                        .onChanged { _ in setPressed(true) }
                        .onEnded { _ in setPressed(false) }
                )
                .position(x: hitRect.midX, y: hitRect.midY)

            visualFace
                .frame(width: visualRect.width, height: visualRect.height)
                .position(x: visualRect.midX, y: visualRect.midY)
                .allowsHitTesting(false)
                .opacity(padOpacity)
        }
        .onDisappear { setPressed(false) }
        .task(id: normalPath ?? "") {
            normalImage = normalPath.flatMap { SkinManifestRuntimeLayout.image(forRelativePath: $0, in: assetsDirectory) }
        }
        .task(id: pressedPath ?? "") {
            pressedImage = pressedPath.flatMap { SkinManifestRuntimeLayout.image(forRelativePath: $0, in: assetsDirectory) }
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(accessibilityName)
        .accessibilityAddTraits(.isButton)
    }

    @ViewBuilder
    private var visualFace: some View {
        if on, let pressed = pressedImage ?? normalImage {
            Image(uiImage: pressed)
                .resizable()
                .interpolation(.high)
                .antialiased(true)
                .scaledToFit()
                .brightness(0.12)
                .scaleEffect(0.94)
        } else if let normal = normalImage {
            Image(uiImage: normal)
                .resizable()
                .interpolation(.high)
                .antialiased(true)
                .scaledToFit()
        } else {
            ControllerAssetImage(
                fileName: ControllerAsset.fileName(for: button),
                fallback: symbol,
                fallbackColor: on ? .white : .white.opacity(0.85),
                fallbackFontSize: min(visualRect.width, visualRect.height) * 0.4,
                skin: .custom,
                descriptor: descriptor
            )
            .brightness(on ? 0.18 : 0)
            .scaleEffect(on ? 0.92 : 1.0)
        }
    }

    private func setPressed(_ pressed: Bool) {
        guard on != pressed else { return }
        on = pressed
        EmulatorBridge.shared.setPadButton(button, pressed: pressed)
        if pressed, SettingsStore.shared.hapticFeedback {
            HapticManager.medium.impactOccurred()
        }
    }
}
