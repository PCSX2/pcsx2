// SkinManifestV2.swift — Advanced skin manifest v2 model and validator
// SPDX-License-Identifier: GPL-3.0+
//
// Foundational data model for the ARMSX2 advanced skin-maker system. The schema
// and validator live here; import/storage, UI, and runtime rendering consume
// these types elsewhere. The normalized conversions and representation selector
// below are used by the runtime renderer. Existing skins and layouts are unchanged.

import Foundation
import CoreGraphics

// MARK: - Top-level manifest

/// Codable mirror of the ARMSX2 skin manifest v2 JSON.
///
/// Every field is optional so a structurally-valid-but-semantically-empty
/// document still decodes; ``SkinManifestValidator`` enforces the required
/// fields and reports friendly, JSON-path-located errors. The `representations`
/// map is decoded as raw keyed dictionaries so unknown device or screen-class
/// keys can be reported by the validator instead of being silently dropped.
struct SkinManifestV2: Codable, Equatable {
    var formatVersion: Int?
    var name: String?
    var identifier: String?
    var author: String?
    var version: String?
    var gameTypeIdentifier: String?
    var supportedGameTypeIdentifiers: [String]?
    var preview: String?
    var debug: Bool?
    var representations: [String: [String: SkinOrientationSet]]?

    /// Format versions this model understands. Other versions are rejected by
    /// the validator with a clear message rather than misinterpreted.
    static let supportedFormatVersions: Set<Int> = [2]

    /// Canonical PS2 game-type identifier.
    static let ps2GameTypeIdentifier = "com.armsx2.game.ps2"

    /// Decode a manifest from raw JSON. A throw here means the bytes are not
    /// valid JSON for the v2 shape; semantic problems are left to the validator.
    static func decode(from data: Data) throws -> SkinManifestV2 {
        try JSONDecoder().decode(SkinManifestV2.self, from: data)
    }
}

// MARK: - Representations

/// A pair of orientations for one device + screen class.
struct SkinOrientationSet: Codable, Equatable {
    var portrait: SkinOrientationRepresentation?
    var landscape: SkinOrientationRepresentation?
}

/// Per-orientation layout data.
struct SkinOrientationRepresentation: Codable, Equatable {
    /// Point-based coordinate space that every frame is measured against.
    var mappingSize: SkinSize?
    /// Background art for this orientation.
    var assets: SkinAssets?
    /// Whether this orientation supports the opacity/translucency feature.
    var translucent: Bool?
    /// Reserved: parsed but ignored in manifest v2.0. Screen-frame/bezel
    /// placement is intentionally not supported in this version.
    var screen: SkinFrame?
    /// Positioned controls overlaid on the background.
    var controls: [SkinControl]?
    /// Default touch-target inflation applied to every control in this
    /// orientation unless a control overrides it.
    var extendedEdges: SkinExtendedEdges?
}

/// Background assets for one orientation. Either a single scalable asset
/// (typically a vector PDF) or three raster sizes.
struct SkinAssets: Codable, Equatable {
    var resizable: String?
    var small: String?
    var medium: String?
    var large: String?

    var referencedPaths: [String] {
        var paths: [String] = []
        for candidate in [resizable, small, medium, large] {
            if let path = candidate?.trimmingCharacters(in: .whitespacesAndNewlines), !path.isEmpty {
                paths.append(path)
            }
        }
        return paths
    }
}

/// A point-space rectangle.
struct SkinFrame: Codable, Equatable {
    var x: Double?
    var y: Double?
    var width: Double?
    var height: Double?
}

/// A point-space size.
struct SkinSize: Codable, Equatable {
    var width: Double?
    var height: Double?
}

/// Per-direction touch-target inflation. These expand where a control can be
/// touched without changing its visual frame.
struct SkinExtendedEdges: Codable, Equatable {
    var top: Double?
    var bottom: Double?
    var left: Double?
    var right: Double?
}

/// Moving knob art for a thumbstick control.
struct SkinThumbstick: Codable, Equatable {
    var name: String?
    var width: Double?
    var height: Double?
}

/// Reserved animation descriptor (parsed, inert in manifest v2.0).
struct SkinAnimation: Codable, Equatable {
    var type: String?
    var begin: SkinFrame?
    var end: SkinFrame?
}

// MARK: - Control

/// A single positioned control within an orientation.
struct SkinControl: Codable, Equatable {
    /// Stable, representation-unique identifier.
    var id: String?
    /// Control style. Parsed loosely through ``resolvedKind``.
    var kind: String?
    var frame: SkinFrame?
    var extendedEdges: SkinExtendedEdges?
    var asset: SkinControlAsset?
    /// Optional per-direction normal/pressed art for D-pad controls.
    var directionAssets: SkinDirectionAssets?
    /// Polymorphic inputs value (string, array, or {up,down,left,right} map).
    var inputs: SkinInputs?
    /// Optional L3/R3 click input for thumbstick controls.
    var clickInput: String?
    /// Moving-knob art for thumbstick controls.
    var thumbstick: SkinThumbstick?
    /// Render order; higher draws above lower.
    var zIndex: Double?
    /// Visibility mode. Parsed loosely through ``resolvedVisibility``.
    var visibility: String?
    /// Reserved: parsed, inert in manifest v2.0.
    var haptic: String?
    /// Reserved: parsed, inert in manifest v2.0.
    var sound: String?
    /// Reserved: parsed, inert in manifest v2.0.
    var animation: SkinAnimation?
    /// Future function-button action. Defined but never executed in this phase.
    var action: String?

    var resolvedKind: SkinControlKind { SkinControlKind.from(kind ?? "") }
    var resolvedVisibility: SkinVisibility { SkinVisibility.from(visibility ?? "") }
}

/// Per-state art for a control.
struct SkinControlAsset: Codable, Equatable {
    var normal: String?
    var pressed: String?
    /// Reserved: parsed, inert in manifest v2.0.
    var selected: String?

    var referencedPaths: [String] {
        var paths: [String] = []
        for candidate in [normal, pressed, selected] {
            if let path = candidate?.trimmingCharacters(in: .whitespacesAndNewlines), !path.isEmpty {
                paths.append(path)
            }
        }
        return paths
    }
}

/// Optional per-direction normal/pressed art for a single D-pad direction.
struct SkinDirectionalAsset: Codable, Equatable {
    var normal: String?
    var pressed: String?

    var referencedPaths: [String] {
        [normal, pressed].compactMap { path in
            guard let path else { return nil }
            let trimmed = path.trimmingCharacters(in: .whitespacesAndNewlines)
            return trimmed.isEmpty ? nil : trimmed
        }
    }
}

/// Optional per-direction art for a D-pad control. Any direction may be omitted;
/// omitted directions fall back to the whole-D-pad art.
struct SkinDirectionAssets: Codable, Equatable {
    var up: SkinDirectionalAsset?
    var down: SkinDirectionalAsset?
    var left: SkinDirectionalAsset?
    var right: SkinDirectionalAsset?

    /// Every non-empty normal/pressed path across all four directions.
    var referencedPaths: [String] {
        [up, down, left, right].flatMap { $0?.referencedPaths ?? [] }
    }
}

/// Polymorphic `inputs` value. Button controls use a list of input IDs,
/// composite D-pad/thumbstick controls use a map of slot to input ID, and some
/// control styles use a single string. Decoding never throws: an unrecognized
/// shape decodes to ``unknown`` so the validator can report it.
enum SkinInputs: Codable, Equatable {
    case list([String])
    case map([String: String])
    case single(String)
    case unknown

    init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        if let string = try? container.decode(String.self) {
            self = .single(string)
        } else if let array = try? container.decode([String].self) {
            self = .list(array)
        } else if let dictionary = try? container.decode([String: String].self) {
            self = .map(dictionary)
        } else {
            self = .unknown
        }
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        switch self {
        case .list(let array):      try container.encode(array)
        case .map(let dictionary):  try container.encode(dictionary)
        case .single(let string):   try container.encode(string)
        case .unknown:              try container.encodeNil()
        }
    }

    /// Every input ID referenced by this value, regardless of shape.
    var referencedInputIDs: [String] {
        switch self {
        case .list(let array):     return array
        case .single(let string):  return [string]
        case .map(let dictionary): return Array(dictionary.values)
        case .unknown:             return []
        }
    }
}

// MARK: - Enums

/// Control style, parsed leniently from the raw `kind` string.
enum SkinControlKind: Equatable {
    case button
    case dpad
    case thumbstick
    case function
    case unknown

    var isKnown: Bool { self != .unknown }

    static func from(_ raw: String) -> SkinControlKind {
        switch raw.lowercased() {
        case "button":                          return .button
        case "dpad", "d-pad":                   return .dpad
        case "thumbstick", "stick", "analog":   return .thumbstick
        case "function", "fn":                  return .function
        default:                                return .unknown
        }
    }
}

/// Visibility mode, parsed leniently from the raw `visibility` string.
enum SkinVisibility: Equatable {
    case always
    case hidden
    case autoHideWithPad
    case unknown

    var isKnown: Bool { self != .unknown }

    static func from(_ raw: String) -> SkinVisibility {
        switch raw {
        case "", "always":          return .always
        case "hidden":              return .hidden
        case "autoHideWithPad":     return .autoHideWithPad
        default:                    return .unknown
        }
    }
}

/// Device class, used by the validator and the selection helper.
enum SkinDevice: Equatable {
    case iphone
    case ipad
    case unknown

    static func from(_ raw: String) -> SkinDevice {
        switch raw.lowercased() {
        case "iphone":  return .iphone
        case "ipad":    return .ipad
        default:        return .unknown
        }
    }
}

/// Screen class, used by the validator and the selection helper.
enum SkinScreenClass: Equatable {
    case standard
    case edgeToEdge
    case splitView
    case unknown

    static func from(_ raw: String) -> SkinScreenClass {
        switch raw {
        case "standard":    return .standard
        case "edgeToEdge":  return .edgeToEdge
        case "splitView":   return .splitView
        default:            return .unknown
        }
    }
}

/// Orientation, used by the validator and the selection helper.
enum SkinOrientation: Equatable {
    case portrait
    case landscape
    case unknown

    static func from(_ raw: String) -> SkinOrientation {
        switch raw.lowercased() {
        case "portrait":    return .portrait
        case "landscape":   return .landscape
        default:            return .unknown
        }
    }
}

/// Every PS2 input identifier a manifest control may reference.
enum SkinInputID {
    static let known: Set<String> = [
        // D-pad
        "up", "down", "left", "right",
        // Face buttons (Sony layout)
        "cross", "circle", "square", "triangle",
        // Shoulders and clicks
        "l1", "l2", "l3", "r1", "r2", "r3",
        // System
        "start", "select",
        // Left analog
        "leftAnalogUp", "leftAnalogDown", "leftAnalogLeft", "leftAnalogRight",
        // Right analog
        "rightAnalogUp", "rightAnalogDown", "rightAnalogLeft", "rightAnalogRight"
    ]

    static func isKnown(_ id: String) -> Bool { known.contains(id) }
}

/// Function-button actions reserved for a future phase. These are defined so
/// the validator can accept them, but they are never executed here. Deliberately
/// excludes `screenshot` (no safe bridge action yet) and renderer/resolution
/// actions. Buttons carry a single action only.
enum SkinAction: String, Equatable, CaseIterable {
    case quickSave
    case quickLoad
    case fastForward
    case toggleFastForward
    case pauseResume
    case quit
    case restart
    case saveStates
    case cheats
    case settings
    case toggleControls
    case toggleHaptics
    case toggleOverlay
    case mute
    case retroAchievements
    case openSkinPicker
    case openLayoutEditor

    static let knownIDs: Set<String> = Set(allCases.map { $0.rawValue })

    static func isKnown(_ id: String) -> Bool { knownIDs.contains(id) }
}

// MARK: - Path-safety helper

enum SkinAssetPath {
    /// True when `path` is a safe relative reference for a bundled asset:
    /// non-empty, not absolute, no drive/scheme, no parent-directory traversal,
    /// and no control characters.
    static func isSafeRelative(_ path: String) -> Bool {
        let trimmed = path.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return false }
        if trimmed.hasPrefix("/") || trimmed.hasPrefix("\\") { return false }
        if trimmed.contains(":") { return false }
        let components = trimmed.split(separator: "/", omittingEmptySubsequences: true)
        for component in components {
            let segment = String(component)
            if segment == ".." || segment == "." { return false }
            if segment.contains("\\") { return false }
            for scalar in segment.unicodeScalars where scalar.value < 0x20 || scalar.value == 0x7F {
                return false
            }
        }
        return true
    }
}

// MARK: - Normalized conversions (reserved for future rendering phases)

/// A frame expressed as fractions (0...1) of a mapping size.
struct NormalizedFrame: Equatable {
    var x: CGFloat
    var y: CGFloat
    var width: CGFloat
    var height: CGFloat
}

/// Per-direction insets expressed as fractions of a mapping size.
struct NormalizedInsets: Equatable {
    var top: CGFloat
    var bottom: CGFloat
    var left: CGFloat
    var right: CGFloat
}

extension SkinFrame {
    /// Converts this point-space frame to normalized fractions relative to
    /// `mappingSize`. Returns nil when the mapping size is not a positive point
    /// space or any component is missing/non-finite. Used by the runtime renderer.
    func normalized(against mappingSize: SkinSize) -> NormalizedFrame? {
        guard let mappingWidth = mappingSize.width,
              let mappingHeight = mappingSize.height,
              mappingWidth > 0, mappingHeight > 0 else { return nil }
        guard let x = self.x, let y = self.y,
              let width = self.width, let height = self.height,
              x.isFinite, y.isFinite, width.isFinite, height.isFinite else { return nil }
        return NormalizedFrame(
            x: CGFloat(x / mappingWidth),
            y: CGFloat(y / mappingHeight),
            width: CGFloat(width / mappingWidth),
            height: CGFloat(height / mappingHeight)
        )
    }
}

extension SkinExtendedEdges {
    /// Converts these point-space insets to normalized fractions relative to
    /// `mappingSize`. Returns nil when the mapping size is invalid. Missing
    /// edges are treated as zero. Used by the runtime renderer.
    func normalizedInsets(against mappingSize: SkinSize) -> NormalizedInsets? {
        guard let mappingWidth = mappingSize.width,
              let mappingHeight = mappingSize.height,
              mappingWidth > 0, mappingHeight > 0 else { return nil }
        let top = self.top ?? 0
        let bottom = self.bottom ?? 0
        let left = self.left ?? 0
        let right = self.right ?? 0
        guard [top, bottom, left, right].allSatisfy({ $0.isFinite }) else { return nil }
        return NormalizedInsets(
            top: CGFloat(top / mappingHeight),
            bottom: CGFloat(bottom / mappingHeight),
            left: CGFloat(left / mappingWidth),
            right: CGFloat(right / mappingWidth)
        )
    }
}

// MARK: - Representation selection (pure helper, not wired into runtime)

/// Query used to select the best matching representation.
struct SkinManifestQuery: Equatable {
    var device: SkinDevice
    var screenClass: SkinScreenClass
    var orientation: SkinOrientation

    init(
        device: SkinDevice = .iphone,
        screenClass: SkinScreenClass = .standard,
        orientation: SkinOrientation = .landscape
    ) {
        self.device = device
        self.screenClass = screenClass
        self.orientation = orientation
    }
}

extension SkinManifestV2 {
    /// A representation selected by ``bestRepresentation(for:)``.
    struct ResolvedRepresentation: Equatable {
        let device: String
        let screenClass: String
        let orientation: String
        let representation: SkinOrientationRepresentation
    }

    /// Selects the best representation for a query, following a
    /// device/screen-class/orientation fallback chain and finally any available
    /// representation. Used by the runtime renderer to pick the best layout for
    /// the current device, screen class, and orientation.
    func bestRepresentation(for query: SkinManifestQuery) -> ResolvedRepresentation? {
        let available = representations ?? [:]
        for device in Self.orderedRawDevices(preferred: query.device) {
            guard let byScreenClass = available[device] else { continue }
            for screenClass in Self.orderedRawScreenClasses(preferred: query.screenClass) {
                guard let set = byScreenClass[screenClass] else { continue }
                for orientation in Self.orderedRawOrientations(preferred: query.orientation) {
                    if orientation == "portrait", let portrait = set.portrait {
                        return ResolvedRepresentation(device: device, screenClass: screenClass, orientation: orientation, representation: portrait)
                    }
                    if orientation == "landscape", let landscape = set.landscape {
                        return ResolvedRepresentation(device: device, screenClass: screenClass, orientation: orientation, representation: landscape)
                    }
                }
            }
        }
        for (device, byScreenClass) in available {
            for (screenClass, set) in byScreenClass {
                if let portrait = set.portrait {
                    return ResolvedRepresentation(device: device, screenClass: screenClass, orientation: "portrait", representation: portrait)
                }
                if let landscape = set.landscape {
                    return ResolvedRepresentation(device: device, screenClass: screenClass, orientation: "landscape", representation: landscape)
                }
            }
        }
        return nil
    }

    private static func orderedRawDevices(preferred: SkinDevice) -> [String] {
        preferred == .ipad ? ["ipad", "iphone"] : ["iphone", "ipad"]
    }

    private static func orderedRawScreenClasses(preferred: SkinScreenClass) -> [String] {
        preferred == .edgeToEdge ? ["edgeToEdge", "standard"] : ["standard", "edgeToEdge"]
    }

    private static func orderedRawOrientations(preferred: SkinOrientation) -> [String] {
        preferred == .portrait ? ["portrait", "landscape"] : ["landscape", "portrait"]
    }
}

// MARK: - Validator

/// Validates a decoded manifest without installing it. Produces structured
/// errors and warnings with JSON-path-like locations.
enum SkinManifestValidator {
    struct Issue: Equatable {
        let location: String
        let message: String

        var description: String { "\(location): \(message)" }
    }

    struct Result: Equatable {
        var errors: [Issue] = []
        var warnings: [Issue] = []

        var isValid: Bool { errors.isEmpty }
    }

    /// Convenience entry point: decode then validate. A decode failure is
    /// reported as a single root error rather than throwing.
    static func validate(jsonData: Data, packageRoot: URL? = nil) -> Result {
        guard let manifest = try? SkinManifestV2.decode(from: jsonData) else {
            return Result(errors: [Issue(location: "(root)", message: "the manifest is not valid JSON or does not match the v2 shape")])
        }
        return validate(manifest, packageRoot: packageRoot)
    }

    /// Validates an already-decoded manifest. When `packageRoot` is supplied,
    /// referenced asset and preview paths are also checked for existence
    /// (reported as warnings); without it, validation is structure-only.
    static func validate(_ manifest: SkinManifestV2, packageRoot: URL? = nil) -> Result {
        var result = Result()

        // formatVersion
        if let version = manifest.formatVersion {
            if !SkinManifestV2.supportedFormatVersions.contains(version) {
                let supported = SkinManifestV2.supportedFormatVersions.sorted().map(String.init).joined(separator: ", ")
                result.errors.append(Issue(location: "formatVersion", message: "unsupported format version \(version); supported: \(supported)"))
            }
        } else {
            result.errors.append(Issue(location: "formatVersion", message: "missing"))
        }

        // name
        if (manifest.name ?? "").trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            result.errors.append(Issue(location: "name", message: "missing or empty"))
        }

        // identifier
        let identifier = (manifest.identifier ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        if identifier.isEmpty {
            result.errors.append(Issue(location: "identifier", message: "missing or empty"))
        } else if !Self.isSafeIdentifier(identifier) {
            result.errors.append(Issue(location: "identifier", message: "must contain only letters, digits, dots, dashes, or underscores (reverse-DNS recommended)"))
        }

        // game type
        let primaryType = (manifest.gameTypeIdentifier ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        let supportedTypes = (manifest.supportedGameTypeIdentifiers ?? []).compactMap { entry -> String? in
            let trimmed = entry.trimmingCharacters(in: .whitespacesAndNewlines)
            return trimmed.isEmpty ? nil : trimmed
        }
        let declaredTypes = ([primaryType] + supportedTypes).filter { !$0.isEmpty }
        if declaredTypes.isEmpty {
            result.errors.append(Issue(location: "gameTypeIdentifier", message: "missing; a PS2 game type is required"))
        } else if !declaredTypes.contains(where: { Self.isPS2GameType($0) }) {
            result.errors.append(Issue(location: "gameTypeIdentifier", message: "not a PS2 skin; expected \"\(SkinManifestV2.ps2GameTypeIdentifier)\" (declared: \(declaredTypes.joined(separator: ", ")))"))
        }

        // preview (optional)
        if let preview = manifest.preview?.trimmingCharacters(in: .whitespacesAndNewlines), !preview.isEmpty {
            if !SkinAssetPath.isSafeRelative(preview) {
                result.errors.append(Issue(location: "preview", message: "preview path \"\(preview)\" is not a safe relative path"))
            } else if let packageRoot, !FileManager.default.fileExists(atPath: packageRoot.appendingPathComponent(preview).path) {
                result.warnings.append(Issue(location: "preview", message: "preview \"\(preview)\" was not found in the package"))
            }
        }

        // representations
        guard let representations = manifest.representations, !representations.isEmpty else {
            result.errors.append(Issue(location: "representations", message: "missing or empty; at least one representation is required"))
            return result
        }

        for deviceKey in representations.keys.sorted() {
            let devicePath = "representations.\(deviceKey)"
            if SkinDevice.from(deviceKey) == .unknown {
                result.warnings.append(Issue(location: devicePath, message: "unsupported device \"\(deviceKey)\"; supported: iphone, ipad"))
            }
            guard let byScreenClass = representations[deviceKey], !byScreenClass.isEmpty else {
                result.errors.append(Issue(location: devicePath, message: "no screen classes defined"))
                continue
            }
            for screenClassKey in byScreenClass.keys.sorted() {
                let screenClassPath = "\(devicePath).\(screenClassKey)"
                let screenClass = SkinScreenClass.from(screenClassKey)
                if screenClass == .unknown {
                    result.warnings.append(Issue(location: screenClassPath, message: "unsupported screen class \"\(screenClassKey)\"; supported: standard, edgeToEdge"))
                } else if screenClass == .splitView {
                    result.warnings.append(Issue(location: screenClassPath, message: "splitView is parsed but reserved; it is ignored in manifest v2.0"))
                }
                guard let orientationSet = byScreenClass[screenClassKey] else { continue }
                if orientationSet.portrait == nil && orientationSet.landscape == nil {
                    result.errors.append(Issue(location: screenClassPath, message: "no orientations defined"))
                    continue
                }
                if let portrait = orientationSet.portrait {
                    validateOrientation(portrait, location: "\(screenClassPath).portrait", packageRoot: packageRoot, into: &result)
                }
                if let landscape = orientationSet.landscape {
                    validateOrientation(landscape, location: "\(screenClassPath).landscape", packageRoot: packageRoot, into: &result)
                }
            }
        }

        return result
    }

    // MARK: Orientation + control checks

    private static func validateOrientation(
        _ representation: SkinOrientationRepresentation,
        location: String,
        packageRoot: URL?,
        into result: inout Result
    ) {
        validateMappingSize(representation.mappingSize, location: location, into: &result)
        validateExtendedEdges(representation.extendedEdges, location: "\(location).extendedEdges", into: &result)

        if let assets = representation.assets {
            validateAssetPaths(assets.referencedPaths, location: "\(location).assets", packageRoot: packageRoot, into: &result)
        }

        if representation.screen != nil {
            result.warnings.append(Issue(location: "\(location).screen", message: "parsed but ignored in manifest v2.0 (screen-frame/bezel placement is not supported yet)"))
        }

        guard let controls = representation.controls, !controls.isEmpty else {
            result.warnings.append(Issue(location: "\(location).controls", message: "no controls defined; this representation is background-only"))
            return
        }

        var seenIdentifiers = Set<String>()
        for (index, control) in controls.enumerated() {
            validateControl(control, location: "\(location).controls[\(index)]", packageRoot: packageRoot, seenIdentifiers: &seenIdentifiers, into: &result)
        }
    }

    private static func validateControl(
        _ control: SkinControl,
        location: String,
        packageRoot: URL?,
        seenIdentifiers: inout Set<String>,
        into result: inout Result
    ) {
        let identifier = (control.id ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        if identifier.isEmpty {
            result.errors.append(Issue(location: "\(location).id", message: "missing or empty"))
        } else if !seenIdentifiers.insert(identifier).inserted {
            result.errors.append(Issue(location: "\(location).id", message: "duplicate control id \"\(identifier)\""))
        }

        let kind = control.resolvedKind
        if !kind.isKnown {
            result.warnings.append(Issue(location: "\(location).kind", message: "unsupported kind \"\(control.kind ?? "")\"; supported: button, dpad, thumbstick, function"))
        }

        validateFrame(control.frame, location: "\(location).frame", requirePositiveSize: true, into: &result)
        validateExtendedEdges(control.extendedEdges, location: "\(location).extendedEdges", into: &result)

        if let zIndex = control.zIndex, !zIndex.isFinite {
            result.errors.append(Issue(location: "\(location).zIndex", message: "must be a finite number"))
        }

        if control.resolvedVisibility == .unknown {
            result.warnings.append(Issue(location: "\(location).visibility", message: "unsupported visibility \"\(control.visibility ?? "")\"; supported: always, hidden, autoHideWithPad"))
        }

        // inputs
        if let inputs = control.inputs {
            for inputID in inputs.referencedInputIDs where !SkinInputID.isKnown(inputID) {
                result.errors.append(Issue(location: "\(location).inputs", message: "unknown input \"\(inputID)\""))
            }
            switch inputs {
            case .list:
                if kind == .dpad || kind == .thumbstick {
                    result.warnings.append(Issue(location: "\(location).inputs", message: "a \(kind) control usually maps inputs as {up, down, left, right}"))
                }
            case .map:
                if kind != .dpad && kind != .thumbstick {
                    result.warnings.append(Issue(location: "\(location).inputs", message: "a \(kind) control usually lists inputs as an array"))
                }
            case .single, .unknown:
                break
            }
        }

        // click input (sticks)
        if let click = control.clickInput?.trimmingCharacters(in: .whitespacesAndNewlines), !click.isEmpty, !SkinInputID.isKnown(click) {
            result.errors.append(Issue(location: "\(location).clickInput", message: "unknown input \"\(click)\""))
        }

        // action (future function buttons; never executed here)
        let action = (control.action ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        if !action.isEmpty {
            if !SkinAction.isKnown(action) {
                result.errors.append(Issue(location: "\(location).action", message: "action \"\(action)\" is not supported in this version"))
            } else if kind != .function {
                result.warnings.append(Issue(location: "\(location).action", message: "action is ignored unless the control kind is \"function\""))
            }
        } else if kind == .function {
            result.warnings.append(Issue(location: "\(location).action", message: "function control has no action"))
        }

        // thumbstick sizing
        if let thumbstick = control.thumbstick {
            if let width = thumbstick.width, !width.isFinite || width <= 0 {
                result.errors.append(Issue(location: "\(location).thumbstick.width", message: "expected a positive number"))
            }
            if let height = thumbstick.height, !height.isFinite || height <= 0 {
                result.errors.append(Issue(location: "\(location).thumbstick.height", message: "expected a positive number"))
            }
        }

        // assets
        if let asset = control.asset {
            validateAssetPaths(asset.referencedPaths, location: "\(location).asset", packageRoot: packageRoot, into: &result)
            if asset.selected != nil {
                result.warnings.append(Issue(location: "\(location).asset.selected", message: "reserved and ignored in manifest v2.0"))
            }
        }

        // Optional per-direction D-pad art. Safe-relative paths are enforced here;
        // existence of referenced files is a blocking install check in the importer.
        if let directions = control.directionAssets {
            for (name, directional) in [("up", directions.up), ("down", directions.down), ("left", directions.left), ("right", directions.right)] {
                if let directional {
                    validateAssetPaths(directional.referencedPaths, location: "\(location).directionAssets.\(name)", packageRoot: packageRoot, into: &result)
                }
            }
        }

        // reserved fields (parsed, inert)
        if control.haptic != nil {
            result.warnings.append(Issue(location: "\(location).haptic", message: "reserved and ignored in manifest v2.0"))
        }
        if control.sound != nil {
            result.warnings.append(Issue(location: "\(location).sound", message: "reserved and ignored in manifest v2.0"))
        }
        if control.animation != nil {
            result.warnings.append(Issue(location: "\(location).animation", message: "reserved and ignored in manifest v2.0"))
        }
    }

    // MARK: Primitive checks

    private static func validateFrame(_ frame: SkinFrame?, location: String, requirePositiveSize: Bool, into result: inout Result) {
        guard let frame else {
            result.errors.append(Issue(location: location, message: "missing frame"))
            return
        }
        for (name, value) in [("x", frame.x), ("y", frame.y), ("width", frame.width), ("height", frame.height)] {
            guard let value else {
                result.errors.append(Issue(location: "\(location).\(name)", message: "missing"))
                continue
            }
            guard value.isFinite else {
                result.errors.append(Issue(location: "\(location).\(name)", message: "must be a finite number"))
                continue
            }
            if (name == "width" || name == "height") && requirePositiveSize && value <= 0 {
                result.errors.append(Issue(location: "\(location).\(name)", message: "expected a positive number"))
            }
        }
    }

    private static func validateExtendedEdges(_ edges: SkinExtendedEdges?, location: String, into result: inout Result) {
        guard let edges else { return }
        for (name, value) in [("top", edges.top), ("bottom", edges.bottom), ("left", edges.left), ("right", edges.right)] {
            guard let value else { continue }
            if !value.isFinite {
                result.errors.append(Issue(location: "\(location).\(name)", message: "must be a finite number"))
            } else if value < 0 {
                result.errors.append(Issue(location: "\(location).\(name)", message: "must not be negative"))
            }
        }
    }

    private static func validateMappingSize(_ size: SkinSize?, location: String, into result: inout Result) {
        guard let size else {
            result.errors.append(Issue(location: "\(location).mappingSize", message: "missing; a positive point space is required"))
            return
        }
        for (name, value) in [("width", size.width), ("height", size.height)] {
            guard let value else {
                result.errors.append(Issue(location: "\(location).mappingSize.\(name)", message: "missing"))
                continue
            }
            guard value.isFinite, value > 0 else {
                result.errors.append(Issue(location: "\(location).mappingSize.\(name)", message: "expected a positive number"))
                continue
            }
        }
    }

    private static func validateAssetPaths(_ paths: [String], location: String, packageRoot: URL?, into result: inout Result) {
        for path in paths {
            let trimmed = path.trimmingCharacters(in: .whitespacesAndNewlines)
            guard !trimmed.isEmpty else { continue }
            if !SkinAssetPath.isSafeRelative(trimmed) {
                result.errors.append(Issue(location: location, message: "asset path \"\(trimmed)\" is not a safe relative path"))
                continue
            }
            if let packageRoot, !FileManager.default.fileExists(atPath: packageRoot.appendingPathComponent(trimmed).path) {
                result.warnings.append(Issue(location: location, message: "asset \"\(trimmed)\" was not found in the package"))
            }
        }
    }

    // MARK: Identifier / game-type checks

    private static func isSafeIdentifier(_ identifier: String) -> Bool {
        guard !identifier.isEmpty else { return false }
        for scalar in identifier.unicodeScalars {
            let value = scalar.value
            let allowed = (value >= 0x30 && value <= 0x39)   // 0-9
                || (value >= 0x41 && value <= 0x5A)           // A-Z
                || (value >= 0x61 && value <= 0x7A)           // a-z
                || value == 0x2E                              // .
                || value == 0x2D                              // -
                || value == 0x5F                              // _
            if !allowed { return false }
        }
        return true
    }

    private static let ps2GameTypeAliases: Set<String> = [
        "com.armsx2.game.ps2",
        "com.armsx2.game.playstation2"
    ]

    private static func isPS2GameType(_ identifier: String) -> Bool {
        ps2GameTypeAliases.contains(identifier.lowercased())
    }
}
