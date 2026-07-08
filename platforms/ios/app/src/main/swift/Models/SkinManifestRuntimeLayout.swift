// SkinManifestRuntimeLayout.swift — Runtime layout for advanced (manifest v2) skins
// SPDX-License-Identifier: GPL-3.0+
//
// Converts a stored manifest-v2.json for an imported skin into a runtime layout
// that VirtualControllerView can render: a background asset plus absolutely
// positioned controls (single buttons, a composite D-pad, thumbsticks). This is
// the Phase 4 rendering bridge; the manifest v2 model and importer (Phases 1-3)
// are unchanged. When a layout cannot be produced the caller falls back to the
// legacy controller rendering, so built-in and legacy imported skins are
// unaffected. Function/action controls are mapped to .inert and never execute;
// screen, sound, animation, haptic, and asset.selected remain parsed/reserved.

import Foundation
import CoreGraphics
import UIKit

/// A resolved, render-ready layout derived from a manifest v2 representation.
struct SkinManifestRuntimeLayout {
    /// Best background asset path (relative to the skin directory), or nil.
    let backgroundAssetPath: String?
    /// Manifest debug flag; when true the controller draws light hit-frame outlines.
    let debug: Bool
    /// Controls in draw order (lower zIndex first).
    let controls: [Control]
    /// Resolved representation mapping size (design canvas). The runtime renderer
    /// shares this with control frames so the background and controls stay aligned.
    let mappingWidth: CGFloat
    let mappingHeight: CGFloat

    struct Control: Identifiable {
        let id: String
        let placement: Placement
        /// Visual frame as fractions (0...1) of the controller canvas.
        let visualFrame: NormalizedFrame
        /// Per-direction hit inflation as fractions of the controller canvas.
        let hitInsets: NormalizedInsets
        let normalAssetPath: String?
        let pressedAssetPath: String?
        /// Optional per-direction normal/pressed art paths for D-pad controls.
        let directional: DirectionalAssetPaths?
        /// Moving-knob art for thumbstick controls.
        let knobAssetPath: String?
        /// Knob size as fractions of the mapping size when the manifest declares
        /// thumbstick width/height; nil falls back to the default knob size.
        let knobWidth: CGFloat?
        let knobHeight: CGFloat?
        let zIndex: Double
    }

    enum Placement {
        /// A single PS2 button placed at its absolute frame.
        case button(ARMSX2PadButton)
        /// A four-cardinal D-pad placed as one composite group at its frame.
        case dpad
        /// A thumbstick (left/right analog) placed at its frame.
        case thumbstick(side: Side, click: ARMSX2PadButton?)
        /// Function/unknown/unmappable control: parsed, never rendered or executed.
        case inert
    }

    enum Side {
        case left
        case right
    }

    /// Resolved per-direction art paths for one D-pad direction.
    struct DirectionalAssetPath: Equatable {
        let normalPath: String?
        let pressedPath: String?
    }

    /// Resolved per-direction art paths for a D-pad control.
    struct DirectionalAssetPaths: Equatable {
        let up: DirectionalAssetPath
        let down: DirectionalAssetPath
        let left: DirectionalAssetPath
        let right: DirectionalAssetPath
    }

    // MARK: - Factory

    /// Builds a runtime layout for `descriptor` if it is an imported manifest v2
    /// skin with a valid representation for the supplied device/orientation.
    /// Returns nil for any non-v2 descriptor or parse failure (legacy fallback).
    /// Pure: device/screen-class detection is done by the caller so this stays
    /// free of UIKit singletons.
    static func make(
        for descriptor: VPadSkinDescriptor,
        isLandscape: Bool,
        device: SkinDevice,
        screenClass: SkinScreenClass,
        skinLibrary: VPadSkinLibraryStore = .shared
    ) -> SkinManifestRuntimeLayout? {
        guard descriptor.source == .imported,
              descriptor.manifestVersion == 2,
              let directory = skinLibrary.importedAssetsDirectory(for: descriptor) else {
            return nil
        }

        let manifestURL = directory.appendingPathComponent(SkinManifestImporter.storedManifestFileName)
        guard let data = try? Data(contentsOf: manifestURL),
              let manifest = try? SkinManifestV2.decode(from: data) else {
            NSLog("[ARMSX2 iOS Skins] v2 manifest could not be read; using legacy controls.")
            return nil
        }

        let query = SkinManifestQuery(
            device: device,
            screenClass: screenClass,
            orientation: isLandscape ? .landscape : .portrait
        )
        guard let resolved = manifest.bestRepresentation(for: query) else {
            NSLog("[ARMSX2 iOS Skins] no matching v2 representation; using legacy controls.")
            return nil
        }
        let representation = resolved.representation

        guard let mappingSize = representation.mappingSize,
              let mappingWidth = mappingSize.width,
              let mappingHeight = mappingSize.height,
              mappingWidth > 0, mappingHeight > 0 else {
            NSLog("[ARMSX2 iOS Skins] v2 representation has no mapping size; using legacy controls.")
            return nil
        }
        let defaultEdges = representation.extendedEdges

        var controls: [Control] = []
        for (index, raw) in (representation.controls ?? []).enumerated() {
            guard let frame = raw.frame,
                  let normalized = frame.normalized(against: mappingSize),
                  normalized.width > 0.001, normalized.height > 0.001,
                  normalized.x.isFinite, normalized.y.isFinite else {
                continue
            }
            let edges = raw.extendedEdges ?? defaultEdges
            let hitInsets = edges?.normalizedInsets(against: mappingSize) ?? NormalizedInsets(top: 0, bottom: 0, left: 0, right: 0)

            let kind = raw.resolvedKind
            let referencedInputs = (raw.inputs?.referencedInputIDs ?? []).filter { SkinInputID.isKnown($0) }
            let normal = Self.trimmedPath(raw.asset?.normal)
            let pressed = Self.trimmedPath(raw.asset?.pressed)
            let knob = Self.trimmedPath(raw.thumbstick?.name)
            let trimmedID = raw.id?.trimmingCharacters(in: .whitespacesAndNewlines)
            let id = (trimmedID?.isEmpty == false) ? trimmedID! : "control-\(index)"

            let placement: Placement
            switch kind {
            case .function, .unknown:
                NSLog("[ARMSX2 iOS Skins] skipping unsupported control '%@' (kind not rendered).", id)
                placement = .inert
            case .thumbstick:
                if referencedInputs.contains(where: { $0.hasPrefix("leftAnalog") }) {
                    placement = .thumbstick(side: .left, click: raw.clickInput.flatMap(Self.mappedButton))
                } else if referencedInputs.contains(where: { $0.hasPrefix("rightAnalog") }) {
                    placement = .thumbstick(side: .right, click: raw.clickInput.flatMap(Self.mappedButton))
                } else {
                    NSLog("[ARMSX2 iOS Skins] thumbstick control '%@' has no analog inputs; skipping.", id)
                    placement = .inert
                }
            case .dpad:
                placement = .dpad
            case .button:
                if let first = referencedInputs.compactMap(Self.mappedButton).first {
                    placement = .button(first)
                } else {
                    NSLog("[ARMSX2 iOS Skins] button control '%@' has no mappable input; skipping.", id)
                    placement = .inert
                }
            }

            controls.append(Control(
                id: id,
                placement: placement,
                visualFrame: normalized,
                hitInsets: hitInsets,
                normalAssetPath: normal,
                pressedAssetPath: pressed,
                directional: Self.directionalPaths(from: raw.directionAssets),
                knobAssetPath: knob,
                knobWidth: Self.knobFraction(raw.thumbstick?.width, mappingDimension: mappingWidth),
                knobHeight: Self.knobFraction(raw.thumbstick?.height, mappingDimension: mappingHeight),
                zIndex: raw.zIndex ?? 0
            ))
        }
        controls.sort { $0.zIndex < $1.zIndex }

        let background = Self.bestBackgroundPath(in: representation.assets, directory: directory)
        return SkinManifestRuntimeLayout(
            backgroundAssetPath: background,
            debug: manifest.debug ?? false,
            controls: controls,
            mappingWidth: CGFloat(mappingWidth),
            mappingHeight: CGFloat(mappingHeight)
        )
    }

    // MARK: - Device detection (MainActor; UIKit singletons)

    /// iPhone on phone idiom, iPad on pad idiom.
    @MainActor
    static func currentDevice() -> SkinDevice {
        UIDevice.current.userInterfaceIdiom == .pad ? .ipad : .iphone
    }

    /// Prefers edgeToEdge on modern full-screen devices (non-zero safe area),
    /// otherwise standard. `bestRepresentation` falls back across the chain, so
    /// this only biases the preferred screen class.
    @MainActor
    static func currentScreenClass() -> SkinScreenClass {
        let scene = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.first
        let insets = scene?.windows.first?.safeAreaInsets ?? .zero
        let hasSafeArea = insets.top > 0 || insets.bottom > 0 || insets.left > 0 || insets.right > 0
        return hasSafeArea ? .edgeToEdge : .standard
    }

    // MARK: - Asset loading

    /// Loads an image for a manifest asset path relative to `directory`. PNG/JPG/
    /// JPEG/WebP are supported now; PDF is reserved (returns nil) because SwiftUI
    /// image loading cannot rasterize it reliably. Unsafe paths return nil.
    static func image(forRelativePath path: String?, in directory: URL) -> UIImage? {
        guard let path, SkinAssetPath.isSafeRelative(path) else { return nil }
        if path.lowercased().hasSuffix(".pdf") {
            NSLog("[ARMSX2 iOS Skins] PDF asset '%@' is reserved and not rendered yet.", path)
            return nil
        }
        let url = directory.appendingPathComponent(path)
        guard FileManager.default.fileExists(atPath: url.path) else { return nil }
        return UIImage(contentsOfFile: url.path)
    }

    // MARK: - Helpers

    private static func trimmedPath(_ value: String?) -> String? {
        guard let trimmed = value?.trimmingCharacters(in: .whitespacesAndNewlines), !trimmed.isEmpty else {
            return nil
        }
        return trimmed
    }

    /// Picks the best existing, raster-supported background asset path.
    private static func bestBackgroundPath(in assets: SkinAssets?, directory: URL) -> String? {
        guard let assets else { return nil }
        for path in assets.referencedPaths {
            guard SkinAssetPath.isSafeRelative(path), !path.lowercased().hasSuffix(".pdf") else { continue }
            if FileManager.default.fileExists(atPath: directory.appendingPathComponent(path).path) {
                return path
            }
        }
        return nil
    }

    /// Maps a manifest input id to a single PS2 button, or nil for analog axes.
    private static func mappedButton(_ input: String) -> ARMSX2PadButton? {
        switch input {
        case "up": return .up
        case "down": return .down
        case "left": return .left
        case "right": return .right
        case "cross": return .cross
        case "circle": return .circle
        case "square": return .square
        case "triangle": return .triangle
        case "l1": return .L1
        case "l2": return .L2
        case "r1": return .R1
        case "r2": return .R2
        case "start": return .start
        case "select": return .select
        case "l3": return .L3
        case "r3": return .R3
        default: return nil
        }
    }

    /// Converts a manifest thumbstick dimension to a fraction of the mapping
    /// size, or nil when the value is absent, non-positive, or non-finite.
    private static func knobFraction(_ value: Double?, mappingDimension: Double) -> CGFloat? {
        guard let value, value > 0, value.isFinite, mappingDimension > 0 else { return nil }
        return CGFloat(value / mappingDimension)
    }

    /// Resolves optional per-direction D-pad art paths. Returns nil when the
    /// manifest omits directionAssets or when no direction supplies any art.
    private static func directionalPaths(from raw: SkinDirectionAssets?) -> DirectionalAssetPaths? {
        guard let raw else { return nil }
        func make(_ d: SkinDirectionalAsset?) -> DirectionalAssetPath {
            DirectionalAssetPath(normalPath: trimmedPath(d?.normal), pressedPath: trimmedPath(d?.pressed))
        }
        let paths = DirectionalAssetPaths(up: make(raw.up), down: make(raw.down), left: make(raw.left), right: make(raw.right))
        let hasAny = [paths.up, paths.down, paths.left, paths.right].contains(where: { $0.normalPath != nil || $0.pressedPath != nil })
        return hasAny ? paths : nil
    }
}
