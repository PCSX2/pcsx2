// PadLayoutStore.swift — INI-backed virtual pad layout positions
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct PadGroupPosition: Codable, Equatable {
    var x: CGFloat
    var y: CGFloat
    // Per-axis sizing is the source of truth. `scale`/`hitScale` are uniform
    // accessors retained for backward compatibility: reading returns the X axis
    // (the representative value), writing sets BOTH axes (the linked behavior).
    var scaleX: CGFloat
    var scaleY: CGFloat
    var hitScaleX: CGFloat
    var hitScaleY: CGFloat

    var scale: CGFloat {
        get { scaleX }
        set { scaleX = newValue; scaleY = newValue }
    }
    var hitScale: CGFloat {
        get { hitScaleX }
        set { hitScaleX = newValue; hitScaleY = newValue }
    }

    /// Uniform initializer — all four axes take the same scale (and hit scale).
    /// Existing call sites keep using this and behave exactly as before.
    init(x: CGFloat, y: CGFloat, scale: CGFloat, hitScale: CGFloat? = nil) {
        self.x = x
        self.y = y
        self.scaleX = scale
        self.scaleY = scale
        let h = hitScale ?? scale
        self.hitScaleX = h
        self.hitScaleY = h
    }

    /// Per-axis initializer for independently sized controls.
    init(x: CGFloat, y: CGFloat, scaleX: CGFloat, scaleY: CGFloat, hitScaleX: CGFloat, hitScaleY: CGFloat) {
        self.x = x
        self.y = y
        self.scaleX = scaleX
        self.scaleY = scaleY
        self.hitScaleX = hitScaleX
        self.hitScaleY = hitScaleY
    }
}

enum PadSizeKind {
    case visible
    case hit
}

enum PadSizeAxis {
    case x
    case y
}

protocol PadLayoutINIStore: AnyObject {
    func getFloat(_ section: String, key: String, defaultValue: Float) -> Float
    func setFloat(_ section: String, key: String, value: Float)
}

final class ARMSX2BridgePadLayoutINIStore: PadLayoutINIStore {
    func getFloat(_ section: String, key: String, defaultValue: Float) -> Float {
        ARMSX2Bridge.getINIFloat(section, key: key, defaultValue: defaultValue)
    }

    func setFloat(_ section: String, key: String, value: Float) {
        ARMSX2Bridge.setINIFloat(section, key: key, value: value)
    }
}

enum PadLayoutMetrics {
    static let minimumTouchLength: CGFloat = 55
    static let minControlScale: CGFloat = 0.5
    static let maxControlScale: CGFloat = 7.0

    static func visibleLength(baseLength: CGFloat, visibleScale: CGFloat) -> CGFloat {
        baseLength * visibleScale
    }

    static func touchLength(baseLength: CGFloat, hitScale: CGFloat) -> CGFloat {
        max(baseLength, minimumTouchLength) * hitScale
    }

    static func clampedScale(_ scale: CGFloat) -> CGFloat {
        guard scale.isFinite else { return 1.0 }
        return min(max(scale, minControlScale), maxControlScale)
    }
}

// MARK: - Shared per-button offset constants
// These must match the exact offset math used in DPadView and ActionButtonsView.
enum VirtualPadButtonOffset {
    static let dpadPortraitSize: CGFloat = 100
    static let dpadLandscapeSize: CGFloat = 110
    static let actionButtonSize: CGFloat = 42

    static func dpadButtonWidth(isLandscape: Bool) -> CGFloat {
        (isLandscape ? dpadLandscapeSize : dpadPortraitSize) * 0.42
    }

    static func dpadOffset(isLandscape: Bool) -> CGFloat {
        (isLandscape ? dpadLandscapeSize : dpadPortraitSize) * 0.29
    }

    static let actionOffset: CGFloat = actionButtonSize * 1.1

    static func offset(for buttonID: String, isLandscape: Bool) -> CGSize {
        let dpadOff = dpadOffset(isLandscape: isLandscape)
        let actionOff = actionOffset
        switch buttonID {
        case "up":       return CGSize(width: 0, height: -dpadOff)
        case "down":     return CGSize(width: 0, height: dpadOff)
        case "left":     return CGSize(width: -dpadOff, height: 0)
        case "right":    return CGSize(width: dpadOff, height: 0)
        case "triangle": return CGSize(width: 0, height: -actionOff)
        case "cross":    return CGSize(width: 0, height: actionOff)
        case "square":   return CGSize(width: -actionOff, height: 0)
        case "circle":   return CGSize(width: actionOff, height: 0)
        default:         return .zero
        }
    }
}

@Observable
final class PadLayoutStore: @unchecked Sendable {
    static let shared = PadLayoutStore()
    private let iniStore: PadLayoutINIStore

    static let actionButtonIDs = ["cross", "circle", "square", "triangle"]
    static let perButtonIDs = ["triangle", "circle", "square", "cross", "up", "down", "left", "right"]
    static let groupIDs = ["dpad", "action", "l1", "l2", "r1", "r2", "lstick", "rstick", "select", "start"]

    var portrait: [String: PadGroupPosition] = [:]
    var landscape: [String: PadGroupPosition] = [:]

    // Per-button overrides — only populated when the user moves an individual button.
    var perButtonPortrait: [String: PadGroupPosition] = [:]
    var perButtonLandscape: [String: PadGroupPosition] = [:]

    // Group-level control visibility. Keys are group IDs; value `false` means hidden.
    // Absent key means visible (default). Stored globally, not per-orientation.
    var controlVisibility: [String: Bool] = [:]

    // MARK: - Default positions (derived from current hardcoded layout)

    // Portrait: relative to controller area (0.0-1.0)
    // U002: action x adjusted from 0.88/0.92 to 0.85/0.88 to prevent ○ button clipping
    static let defaultPortrait: [String: PadGroupPosition] = [
        "l2":     PadGroupPosition(x: 0.16, y: 0.06, scale: 1.0),
        "l1":     PadGroupPosition(x: 0.16, y: 0.14, scale: 1.0),
        "r2":     PadGroupPosition(x: 0.84, y: 0.06, scale: 1.0),
        "r1":     PadGroupPosition(x: 0.84, y: 0.14, scale: 1.0),
        "select": PadGroupPosition(x: 0.43, y: 0.20, scale: 1.0),
        "start":  PadGroupPosition(x: 0.57, y: 0.20, scale: 1.0),
        "dpad":   PadGroupPosition(x: 0.16, y: 0.48, scale: 1.0),
        "action": PadGroupPosition(x: 0.82, y: 0.44, scale: 1.0),
        "lstick": PadGroupPosition(x: 0.28, y: 0.78, scale: 1.0),
        "rstick": PadGroupPosition(x: 0.72, y: 0.78, scale: 1.0),
    ]

    // Landscape: relative to full screen — positions kept well inside safe area
    // to avoid clipping on notch/Dynamic Island devices
    static let defaultLandscape: [String: PadGroupPosition] = [
        "dpad":   PadGroupPosition(x: 0.14, y: 0.72, scale: 1.0),
        "action": PadGroupPosition(x: 0.84, y: 0.72, scale: 1.0),
        "l2":     PadGroupPosition(x: 0.14, y: 0.22, scale: 1.0),
        "l1":     PadGroupPosition(x: 0.14, y: 0.34, scale: 1.0),
        "r2":     PadGroupPosition(x: 0.86, y: 0.22, scale: 1.0),
        "r1":     PadGroupPosition(x: 0.86, y: 0.34, scale: 1.0),
        "select": PadGroupPosition(x: 0.43, y: 0.90, scale: 1.0),
        "start":  PadGroupPosition(x: 0.57, y: 0.90, scale: 1.0),
        "lstick": PadGroupPosition(x: 0.26, y: 0.86, scale: 1.0),
        "rstick": PadGroupPosition(x: 0.68, y: 0.86, scale: 1.0),
    ]

    init(iniStore: PadLayoutINIStore = ARMSX2BridgePadLayoutINIStore(), loadFromStore: Bool = true) {
        self.iniStore = iniStore
        portrait = Self.defaultPortrait
        landscape = Self.defaultLandscape
        if loadFromStore {
            load()
        }
    }

    func position(for id: String, landscape isLandscape: Bool) -> PadGroupPosition {
        let dict = isLandscape ? landscape : portrait
        let defaults = isLandscape ? Self.defaultLandscape : Self.defaultPortrait
        return dict[id] ?? defaults[id] ?? PadGroupPosition(x: 0.5, y: 0.5, scale: 1.0)
    }

    // MARK: - Per-button position lookup

    /// Returns the position for an individual button.
    /// If a per-button override exists, it is returned.
    /// Otherwise, the default is computed from the legacy group position using
    /// the same offset math as the current grouped layout.
    func perButtonPosition(for id: String, landscape: Bool, areaW: CGFloat, areaH: CGFloat) -> PadGroupPosition {
        let dict = landscape ? perButtonLandscape : perButtonPortrait
        if let pos = dict[id] {
            return pos
        }
        let groupID = (id == "triangle" || id == "circle" || id == "square" || id == "cross") ? "action" : "dpad"
        let groupPos = position(for: groupID, landscape: landscape)
        return defaultPerButtonPosition(for: id, groupPos: groupPos, isLandscape: landscape, areaW: areaW, areaH: areaH)
    }

    func updateGroupScale(_ id: String, scale: CGFloat, landscape isLandscape: Bool) {
        var p = position(for: id, landscape: isLandscape)
        p.scale = PadLayoutMetrics.clampedScale(scale)
        setGroupPosition(p, for: id, landscape: isLandscape)
    }

    func updateGroupHitScale(_ id: String, hitScale: CGFloat, landscape isLandscape: Bool) {
        var p = position(for: id, landscape: isLandscape)
        p.hitScale = PadLayoutMetrics.clampedScale(hitScale)
        setGroupPosition(p, for: id, landscape: isLandscape)
    }

    func updatePerButtonScale(_ id: String, scale: CGFloat, landscape: Bool, areaW: CGFloat, areaH: CGFloat) {
        var p = perButtonPosition(for: id, landscape: landscape, areaW: areaW, areaH: areaH)
        p.scale = PadLayoutMetrics.clampedScale(scale)
        setPerButtonPosition(p, for: id, landscape: landscape)
    }

    func updatePerButtonHitScale(_ id: String, hitScale: CGFloat, landscape: Bool, areaW: CGFloat, areaH: CGFloat) {
        var p = perButtonPosition(for: id, landscape: landscape, areaW: areaW, areaH: areaH)
        p.hitScale = PadLayoutMetrics.clampedScale(hitScale)
        setPerButtonPosition(p, for: id, landscape: landscape)
    }

    // MARK: - Per-axis (unlinked) sizing

    func updateGroupSize(_ id: String, kind: PadSizeKind, axis: PadSizeAxis, scale: CGFloat, landscape isLandscape: Bool) {
        var p = position(for: id, landscape: isLandscape)
        Self.applyAxisScale(&p, kind: kind, axis: axis, scale: scale)
        setGroupPosition(p, for: id, landscape: isLandscape)
    }

    func updatePerButtonSize(_ id: String, kind: PadSizeKind, axis: PadSizeAxis, scale: CGFloat, landscape: Bool, areaW: CGFloat, areaH: CGFloat) {
        var p = perButtonPosition(for: id, landscape: landscape, areaW: areaW, areaH: areaH)
        Self.applyAxisScale(&p, kind: kind, axis: axis, scale: scale)
        setPerButtonPosition(p, for: id, landscape: landscape)
    }

    private static func applyAxisScale(_ p: inout PadGroupPosition, kind: PadSizeKind, axis: PadSizeAxis, scale: CGFloat) {
        let clamped = PadLayoutMetrics.clampedScale(scale)
        switch (kind, axis) {
        case (.visible, .x): p.scaleX = clamped
        case (.visible, .y): p.scaleY = clamped
        case (.hit, .x):     p.hitScaleX = clamped
        case (.hit, .y):     p.hitScaleY = clamped
        }
    }

    /// Re-lock a control's aspect: collapse the Y axis onto the X axis
    /// (width-wins) for both the visible and hit scales.
    func relinkAxes(_ id: String, perButton: Bool, landscape: Bool, areaW: CGFloat, areaH: CGFloat) {
        if perButton {
            var p = perButtonPosition(for: id, landscape: landscape, areaW: areaW, areaH: areaH)
            p.scaleY = p.scaleX
            p.hitScaleY = p.hitScaleX
            setPerButtonPosition(p, for: id, landscape: landscape)
        } else {
            var p = position(for: id, landscape: landscape)
            p.scaleY = p.scaleX
            p.hitScaleY = p.hitScaleX
            setGroupPosition(p, for: id, landscape: landscape)
        }
    }

    func setGroupPosition(_ position: PadGroupPosition, for id: String, landscape isLandscape: Bool) {
        if isLandscape {
            landscape[id] = position
        } else {
            portrait[id] = position
        }
    }

    func setPerButtonPosition(_ position: PadGroupPosition, for id: String, landscape isLandscape: Bool) {
        if isLandscape {
            perButtonLandscape[id] = position
        } else {
            perButtonPortrait[id] = position
        }
    }

    private func defaultPerButtonPosition(for id: String, groupPos: PadGroupPosition, isLandscape: Bool, areaW: CGFloat, areaH: CGFloat) -> PadGroupPosition {
        let offset = VirtualPadButtonOffset.offset(for: id, isLandscape: isLandscape)
        let scaledOffsetX = offset.width * groupPos.scaleX
        let scaledOffsetY = offset.height * groupPos.scaleY
        return PadGroupPosition(
            x: groupPos.x + scaledOffsetX / areaW,
            y: groupPos.y + scaledOffsetY / areaH,
            scaleX: groupPos.scaleX,
            scaleY: groupPos.scaleY,
            hitScaleX: groupPos.hitScaleX,
            hitScaleY: groupPos.hitScaleY
        )
    }

    static func groupID(for perButtonID: String) -> String {
        if ["triangle", "circle", "square", "cross"].contains(perButtonID) { return "action" }
        if ["up", "down", "left", "right"].contains(perButtonID) { return "dpad" }
        return perButtonID
    }

    func groupID(for perButtonID: String) -> String {
        Self.groupID(for: perButtonID)
    }

    // MARK: - Visibility helpers

    /// Returns whether a control or button is visible.
    /// If an explicit per-button key exists, it wins.
    /// Otherwise, fall back to the group visibility (default: visible).
    func isControlVisible(_ id: String) -> Bool {
        if let explicit = controlVisibility[id] {
            return explicit
        }
        let group = groupID(for: id)
        return controlVisibility[group] ?? true
    }

    func setControlVisible(_ id: String, visible: Bool) {
        if visible {
            let group = groupID(for: id)
            if id == group {
                controlVisibility.removeValue(forKey: id)
            } else if let groupVisible = controlVisibility[group], !groupVisible {
                controlVisibility[id] = true
            } else {
                controlVisibility.removeValue(forKey: id)
            }
        } else {
            controlVisibility[id] = false
        }
    }

    func resetControlVisibility() {
        controlVisibility.removeAll()
    }

    // MARK: - INI persistence

    /// Writes the six sizing keys for a control. Legacy `*_scale`/`*_hitScale`
    /// carry the X axis (representative) so older readers still get a sane value;
    /// the four `*_scaleX/Y`/`*_hitScaleX/Y` keys carry the full per-axis state.
    private func writeSizeKeys(_ pos: PadGroupPosition, section: String, id: String) {
        iniStore.setFloat(section, key: "\(id)_scale", value: Float(pos.scaleX))
        iniStore.setFloat(section, key: "\(id)_hitScale", value: Float(pos.hitScaleX))
        iniStore.setFloat(section, key: "\(id)_scaleX", value: Float(pos.scaleX))
        iniStore.setFloat(section, key: "\(id)_scaleY", value: Float(pos.scaleY))
        iniStore.setFloat(section, key: "\(id)_hitScaleX", value: Float(pos.hitScaleX))
        iniStore.setFloat(section, key: "\(id)_hitScaleY", value: Float(pos.hitScaleY))
    }

    private func writeSizeKeysSentinel(section: String, id: String) {
        for suffix in ["scale", "hitScale", "scaleX", "scaleY", "hitScaleX", "hitScaleY"] {
            iniStore.setFloat(section, key: "\(id)_\(suffix)", value: -1.0)
        }
    }

    func save() {
        for id in Self.groupIDs {
            if let pos = portrait[id] {
                iniStore.setFloat("ARMSX2iOS/PadLayout/Portrait", key: "\(id)_x", value: Float(pos.x))
                iniStore.setFloat("ARMSX2iOS/PadLayout/Portrait", key: "\(id)_y", value: Float(pos.y))
                writeSizeKeys(pos, section: "ARMSX2iOS/PadLayout/Portrait", id: id)
            }
            if let pos = landscape[id] {
                iniStore.setFloat("ARMSX2iOS/PadLayout/Landscape", key: "\(id)_x", value: Float(pos.x))
                iniStore.setFloat("ARMSX2iOS/PadLayout/Landscape", key: "\(id)_y", value: Float(pos.y))
                writeSizeKeys(pos, section: "ARMSX2iOS/PadLayout/Landscape", id: id)
            }
        }
        // Per-button positions: write override if present, sentinel -1 otherwise.
        for id in Self.perButtonIDs {
            if let pos = perButtonPortrait[id] {
                iniStore.setFloat("ARMSX2iOS/PadLayout/PerButtonPortrait", key: "\(id)_x", value: Float(pos.x))
                iniStore.setFloat("ARMSX2iOS/PadLayout/PerButtonPortrait", key: "\(id)_y", value: Float(pos.y))
                writeSizeKeys(pos, section: "ARMSX2iOS/PadLayout/PerButtonPortrait", id: id)
            } else {
                iniStore.setFloat("ARMSX2iOS/PadLayout/PerButtonPortrait", key: "\(id)_x", value: -1.0)
                iniStore.setFloat("ARMSX2iOS/PadLayout/PerButtonPortrait", key: "\(id)_y", value: -1.0)
                writeSizeKeysSentinel(section: "ARMSX2iOS/PadLayout/PerButtonPortrait", id: id)
            }
            if let pos = perButtonLandscape[id] {
                iniStore.setFloat("ARMSX2iOS/PadLayout/PerButtonLandscape", key: "\(id)_x", value: Float(pos.x))
                iniStore.setFloat("ARMSX2iOS/PadLayout/PerButtonLandscape", key: "\(id)_y", value: Float(pos.y))
                writeSizeKeys(pos, section: "ARMSX2iOS/PadLayout/PerButtonLandscape", id: id)
            } else {
                iniStore.setFloat("ARMSX2iOS/PadLayout/PerButtonLandscape", key: "\(id)_x", value: -1.0)
                iniStore.setFloat("ARMSX2iOS/PadLayout/PerButtonLandscape", key: "\(id)_y", value: -1.0)
                writeSizeKeysSentinel(section: "ARMSX2iOS/PadLayout/PerButtonLandscape", id: id)
            }
        }
        // Visibility: `0` = hidden, `1` = visible. Absent means visible (default).
        for id in Self.groupIDs {
            let isVisible = isControlVisible(id)
            iniStore.setFloat("ARMSX2iOS/PadLayout/ControlVisibility", key: id, value: isVisible ? 1.0 : 0.0)
        }
        // Per-button visibility overrides (only action buttons in this pass).
        for id in Self.actionButtonIDs {
            if let explicit = controlVisibility[id] {
                iniStore.setFloat("ARMSX2iOS/PadLayout/ControlVisibility", key: id, value: explicit ? 1.0 : 0.0)
            } else {
                iniStore.setFloat("ARMSX2iOS/PadLayout/ControlVisibility", key: id, value: -1.0)
            }
        }
    }

    /// Resolves the four sizing axes for a control using the fallback chain:
    ///   scaleX    = *_scaleX    ?? *_scale    ?? 1.0
    ///   scaleY    = *_scaleY    ?? *_scale    ?? 1.0
    ///   hitScaleX = *_hitScaleX ?? *_hitScale ?? scaleX
    ///   hitScaleY = *_hitScaleY ?? *_hitScale ?? scaleY
    /// Scales are clamped to a positive range, so a non-positive read means absent.
    private func readSizeAxes(section: String, id: String) -> (scaleX: CGFloat, scaleY: CGFloat, hitScaleX: CGFloat, hitScaleY: CGFloat) {
        let legacyScale = iniStore.getFloat(section, key: "\(id)_scale", defaultValue: -1)
        let legacyHit = iniStore.getFloat(section, key: "\(id)_hitScale", defaultValue: -1)
        let sx = iniStore.getFloat(section, key: "\(id)_scaleX", defaultValue: -1)
        let sy = iniStore.getFloat(section, key: "\(id)_scaleY", defaultValue: -1)
        let hx = iniStore.getFloat(section, key: "\(id)_hitScaleX", defaultValue: -1)
        let hy = iniStore.getFloat(section, key: "\(id)_hitScaleY", defaultValue: -1)

        let baseScale = PadLayoutMetrics.clampedScale(CGFloat(legacyScale > 0 ? legacyScale : 1.0))
        let scaleX = PadLayoutMetrics.clampedScale(sx > 0 ? CGFloat(sx) : baseScale)
        let scaleY = PadLayoutMetrics.clampedScale(sy > 0 ? CGFloat(sy) : baseScale)
        let legacyHitScale = legacyHit > 0 ? PadLayoutMetrics.clampedScale(CGFloat(legacyHit)) : nil
        let hitScaleX = PadLayoutMetrics.clampedScale(hx > 0 ? CGFloat(hx) : (legacyHitScale ?? scaleX))
        let hitScaleY = PadLayoutMetrics.clampedScale(hy > 0 ? CGFloat(hy) : (legacyHitScale ?? scaleY))
        return (scaleX, scaleY, hitScaleX, hitScaleY)
    }

    func load() {
        for id in Self.groupIDs {
            // Portrait
            let px = iniStore.getFloat("ARMSX2iOS/PadLayout/Portrait", key: "\(id)_x", defaultValue: -1)
            if px >= 0 {
                let py = iniStore.getFloat("ARMSX2iOS/PadLayout/Portrait", key: "\(id)_y", defaultValue: 0.5)
                let axes = readSizeAxes(section: "ARMSX2iOS/PadLayout/Portrait", id: id)
                portrait[id] = PadGroupPosition(x: CGFloat(px), y: CGFloat(py), scaleX: axes.scaleX, scaleY: axes.scaleY, hitScaleX: axes.hitScaleX, hitScaleY: axes.hitScaleY)
            }
            // Landscape
            let lx = iniStore.getFloat("ARMSX2iOS/PadLayout/Landscape", key: "\(id)_x", defaultValue: -1)
            if lx >= 0 {
                let ly = iniStore.getFloat("ARMSX2iOS/PadLayout/Landscape", key: "\(id)_y", defaultValue: 0.5)
                let axes = readSizeAxes(section: "ARMSX2iOS/PadLayout/Landscape", id: id)
                landscape[id] = PadGroupPosition(x: CGFloat(lx), y: CGFloat(ly), scaleX: axes.scaleX, scaleY: axes.scaleY, hitScaleX: axes.hitScaleX, hitScaleY: axes.hitScaleY)
            }
        }
        for id in Self.perButtonIDs {
            // Portrait
            let px = iniStore.getFloat(
                "ARMSX2iOS/PadLayout/PerButtonPortrait",
                key: "\(id)_x",
                defaultValue: -1
            )
            let py = iniStore.getFloat(
                "ARMSX2iOS/PadLayout/PerButtonPortrait",
                key: "\(id)_y",
                defaultValue: -1
            )
            if px >= 0 && py >= 0 {
                let axes = readSizeAxes(section: "ARMSX2iOS/PadLayout/PerButtonPortrait", id: id)
                perButtonPortrait[id] = PadGroupPosition(
                    x: CGFloat(px),
                    y: CGFloat(py),
                    scaleX: axes.scaleX,
                    scaleY: axes.scaleY,
                    hitScaleX: axes.hitScaleX,
                    hitScaleY: axes.hitScaleY
                )
            }
            // Landscape
            let lx = iniStore.getFloat(
                "ARMSX2iOS/PadLayout/PerButtonLandscape",
                key: "\(id)_x",
                defaultValue: -1
            )
            let ly = iniStore.getFloat(
                "ARMSX2iOS/PadLayout/PerButtonLandscape",
                key: "\(id)_y",
                defaultValue: -1
            )
            if lx >= 0 && ly >= 0 {
                let axes = readSizeAxes(section: "ARMSX2iOS/PadLayout/PerButtonLandscape", id: id)
                perButtonLandscape[id] = PadGroupPosition(
                    x: CGFloat(lx),
                    y: CGFloat(ly),
                    scaleX: axes.scaleX,
                    scaleY: axes.scaleY,
                    hitScaleX: axes.hitScaleX,
                    hitScaleY: axes.hitScaleY
                )
            }
        }
        // Visibility: `0` = hidden, `1` = visible, absent = visible (default).
        for id in Self.groupIDs {
            let value = iniStore.getFloat("ARMSX2iOS/PadLayout/ControlVisibility", key: id, defaultValue: -1)
            if value >= 0 {
                controlVisibility[id] = (value > 0.5)
            }
        }
        // Per-button visibility overrides.
        for id in Self.actionButtonIDs {
            let value = iniStore.getFloat("ARMSX2iOS/PadLayout/ControlVisibility", key: id, defaultValue: -1)
            if value >= 0 {
                controlVisibility[id] = (value > 0.5)
            }
        }
    }

    func resetPortrait() {
        portrait = Self.defaultPortrait
    }

    func resetLandscape() {
        landscape = Self.defaultLandscape
    }

    func reset(isLandscape: Bool) {
        if isLandscape { resetLandscape() } else { resetPortrait() }
    }

    func resetPerButtonActionButtons(isLandscape: Bool) {
        if isLandscape {
            for id in ["triangle", "circle", "square", "cross"] {
                perButtonLandscape.removeValue(forKey: id)
            }
        } else {
            for id in ["triangle", "circle", "square", "cross"] {
                perButtonPortrait.removeValue(forKey: id)
            }
        }
    }

    func resetPerButtonDPad(isLandscape: Bool) {
        if isLandscape {
            for id in ["up", "down", "left", "right"] {
                perButtonLandscape.removeValue(forKey: id)
            }
        } else {
            for id in ["up", "down", "left", "right"] {
                perButtonPortrait.removeValue(forKey: id)
            }
        }
    }

    func resetAll() {
        portrait = Self.defaultPortrait
        landscape = Self.defaultLandscape
        perButtonPortrait.removeAll()
        perButtonLandscape.removeAll()
        // Note: controlVisibility is intentionally NOT reset here.
        // Use resetControlVisibility() for that.
    }

    func snapshot() -> PadLayoutSnapshot {
        PadLayoutSnapshot(
            portrait: portrait,
            landscape: landscape,
            perButtonPortrait: perButtonPortrait,
            perButtonLandscape: perButtonLandscape,
            controlVisibility: controlVisibility
        )
    }

    func apply(snapshot: PadLayoutSnapshot) {
        portrait = snapshot.portrait
        landscape = snapshot.landscape
        perButtonPortrait = snapshot.perButtonPortrait
        perButtonLandscape = snapshot.perButtonLandscape
        controlVisibility = snapshot.controlVisibility
    }
}
