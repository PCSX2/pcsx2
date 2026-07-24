// DynamicThumbstickSettings.swift — Persistent virtual-pad gesture configuration
// SPDX-License-Identifier: GPL-3.0+

import Foundation

enum VirtualPadActionButton: Int, CaseIterable, Hashable, Identifiable {
    case leftShoulder
    case rightShoulder
    case leftTrigger
    case rightTrigger
    case faceBottom
    case faceLeft
    case faceRight
    case faceTop
    case up
    case down
    case left
    case right
    case start
    case select
    case leftStick
    case rightStick

    var id: Int { rawValue }

    var title: String {
        switch self {
        case .leftShoulder: return "L1"
        case .rightShoulder: return "R1"
        case .leftTrigger: return "L2"
        case .rightTrigger: return "R2"
        case .faceBottom: return "Cross"
        case .faceLeft: return "Square"
        case .faceRight: return "Circle"
        case .faceTop: return "Triangle"
        case .up: return "Up"
        case .down: return "Down"
        case .left: return "Left"
        case .right: return "Right"
        case .start: return "Start"
        case .select: return "Select"
        case .leftStick: return "L3"
        case .rightStick: return "R3"
        }
    }

    var padButton: ARMSX2PadButton {
        switch self {
        case .leftShoulder: return .L1
        case .rightShoulder: return .R1
        case .leftTrigger: return .L2
        case .rightTrigger: return .R2
        case .faceBottom: return .cross
        case .faceLeft: return .square
        case .faceRight: return .circle
        case .faceTop: return .triangle
        case .up: return .up
        case .down: return .down
        case .left: return .left
        case .right: return .right
        case .start: return .start
        case .select: return .select
        case .leftStick: return .L3
        case .rightStick: return .R3
        }
    }
}

enum VirtualPadThumbstickSide {
    case left
    case right
}

enum DynamicCrosshairType: Int, CaseIterable, Identifiable {
    case classic = 0
    case dot = 1
    case circle = 2
    case circleDot = 3
    case cross = 4
    case chevron = 5
    case brackets = 6
    case diamond = 7
    case shotgun = 8
    case sniper = 9
    case tactical = 10
    case burst = 11
    case fourBoxes = 12
    case triad = 13
    case reactiveDot = 14

    var id: Int { rawValue }

    static let allCases: [DynamicCrosshairType] = [
        .fourBoxes,
        .triad,
        .reactiveDot,
        .classic,
        .dot,
        .circle,
        .circleDot,
        .cross,
        .chevron,
        .brackets,
        .diamond,
        .shotgun,
        .sniper,
        .tactical,
        .burst
    ]

    var title: String {
        switch self {
        case .classic: return "Classic"
        case .dot: return "Dot"
        case .circle: return "Circle"
        case .circleDot: return "Circle + Dot"
        case .cross: return "Full Cross"
        case .chevron: return "Chevron"
        case .brackets: return "Corner Brackets"
        case .diamond: return "Diamond"
        case .shotgun: return "Shotgun"
        case .sniper: return "Sniper Scope"
        case .tactical: return "Tactical"
        case .burst: return "Eight-Point Burst"
        case .fourBoxes: return "Four-Box Reactive"
        case .triad: return "Three-Line Triangle"
        case .reactiveDot: return "Reactive Dot"
        }
    }
}

enum DynamicCrosshairAnimation: Int, CaseIterable, Identifiable {
    case reactive = 0
    case pulse
    case expand
    case rotate
    case recoil
    case orbit
    case focus
    case wave
    case directional
    case elastic
    case parallax
    case velocity
    case stabilizer
    case snap
    case drift
    case tilt
    case bloom
    case directionLock

    var id: Int { rawValue }

    var title: String {
        switch self {
        case .reactive: return "Reactive"
        case .pulse: return "Pulse"
        case .expand: return "Expand"
        case .rotate: return "Rotate"
        case .recoil: return "Recoil"
        case .orbit: return "Orbit"
        case .focus: return "Focus"
        case .wave: return "Wave"
        case .directional: return "Directional"
        case .elastic: return "Elastic"
        case .parallax: return "Parallax"
        case .velocity: return "Velocity"
        case .stabilizer: return "Stabilizer"
        case .snap: return "Snap"
        case .drift: return "Drift"
        case .tilt: return "Tilt"
        case .bloom: return "Bloom"
        case .directionLock: return "Direction Lock"
        }
    }
}

@MainActor
@Observable
final class DynamicThumbstickSettings {
    static let shared = DynamicThumbstickSettings()
    private static let section = "ARMSX2iOS/DynamicThumbsticks"

    var legacyThumbsticks: Bool { didSet { setBool("LegacyThumbsticks", legacyThumbsticks) } }
    var dynamicThumbsticks: Bool { didSet { setBool("DynamicThumbsticks", dynamicThumbsticks) } }
    var swipeCamera: Bool { didSet { setBool("SwipeCamera", swipeCamera) } }
    var gyroscopeCamera: Bool { didSet { setBool("GyroscopeCamera", gyroscopeCamera) } }

    var movementSensitivity: Double { didSet { setDouble("MovementSensitivity", movementSensitivity) } }
    var lookSensitivity: Double { didSet { setDouble("LookSensitivity", lookSensitivity) } }
    var swipeSensitivity: Double { didSet { setDouble("SwipeSensitivity", swipeSensitivity) } }
    var swipeHorizontalSensitivity: Double {
        didSet { setDouble("SwipeHorizontalSensitivity", swipeHorizontalSensitivity) }
    }
    var swipeVerticalSensitivity: Double {
        didSet { setDouble("SwipeVerticalSensitivity", swipeVerticalSensitivity) }
    }
    var swipeSensitivityWhileAimingEnabled: Bool {
        didSet { setBool("SwipeSensitivityWhileAimingEnabled", swipeSensitivityWhileAimingEnabled) }
    }
    var swipeSensitivityWhileAiming: Double {
        didSet { setDouble("SwipeSensitivityWhileAiming", swipeSensitivityWhileAiming) }
    }
    var swipeHorizontalSensitivityWhileAiming: Double {
        didSet { setDouble("SwipeHorizontalSensitivityWhileAiming", swipeHorizontalSensitivityWhileAiming) }
    }
    var swipeVerticalSensitivityWhileAiming: Double {
        didSet { setDouble("SwipeVerticalSensitivityWhileAiming", swipeVerticalSensitivityWhileAiming) }
    }
    var swipeSensitivityWhileNotAimingEnabled: Bool {
        didSet { setBool("SwipeSensitivityWhileNotAimingEnabled", swipeSensitivityWhileNotAimingEnabled) }
    }
    var swipeSensitivityWhileNotAiming: Double {
        didSet { setDouble("SwipeSensitivityWhileNotAiming", swipeSensitivityWhileNotAiming) }
    }
    var swipeHorizontalSensitivityWhileNotAiming: Double {
        didSet { setDouble("SwipeHorizontalSensitivityWhileNotAiming", swipeHorizontalSensitivityWhileNotAiming) }
    }
    var swipeVerticalSensitivityWhileNotAiming: Double {
        didSet { setDouble("SwipeVerticalSensitivityWhileNotAiming", swipeVerticalSensitivityWhileNotAiming) }
    }
    var gyroSensitivity: Double { didSet { setDouble("GyroSensitivity", gyroSensitivity) } }
    var gyroAcceleration: Double { didSet { setDouble("GyroAcceleration", gyroAcceleration) } }
    var gyroSmoothing: Double { didSet { setDouble("GyroSmoothing", gyroSmoothing) } }
    var gyroDeadZone: Double { didSet { setDouble("GyroDeadZone", gyroDeadZone) } }
    var gyroMaximumRate: Double { didSet { setDouble("GyroMaximumRate", gyroMaximumRate) } }
    var invertGyroHorizontal: Bool { didSet { setBool("InvertGyroHorizontal", invertGyroHorizontal) } }
    var invertGyroVertical: Bool { didSet { setBool("InvertGyroVertical", invertGyroVertical) } }

    var thumbstickRadius: Double { didSet { setDouble("ThumbstickRadius", thumbstickRadius) } }
    var deadZone: Double { didSet { setDouble("DeadZone", deadZone) } }
    var thumbstickOpacity: Double { didSet { setDouble("ThumbstickOpacity", thumbstickOpacity) } }
    var baseOpacity: Double { didSet { setDouble("BaseOpacity", baseOpacity) } }
    var trailOpacity: Double { didSet { setDouble("TrailOpacity", trailOpacity) } }
    var activationHaptics: Bool { didSet { setBool("ActivationHaptics", activationHaptics) } }

    var leftThumbstickActionsEnabled: Bool { didSet { setBool("LeftThumbstickActionsEnabled", leftThumbstickActionsEnabled) } }
    var rightThumbstickActionsEnabled: Bool { didSet { setBool("DynamicThumbstickActionsEnabled", rightThumbstickActionsEnabled) } }
    var holdAimWhileSwipe: Bool { didSet { setBool("HoldAimWhileSwipe", holdAimWhileSwipe) } }
    var doubleTapToHoldAim: Bool { didSet { setBool("DoubleTapToHoldAim", doubleTapToHoldAim) } }
    var tapToFire: Bool { didSet { setBool("TapToFire", tapToFire) } }
    var rapidTapFireEnabled: Bool { didSet { setBool("RapidTapFireEnabled", rapidTapFireEnabled) } }
    var releaseFireWhenTouchEnds: Bool { didSet { setBool("ReleaseFireWhenTouchEnds", releaseFireWhenTouchEnds) } }
    var extendFireWhileDragging: Bool { didSet { setBool("ExtendFireWhileDragging", extendFireWhileDragging) } }
    var aimReleaseDelay: Double { didSet { setDouble("AimReleaseDelay", aimReleaseDelay) } }
    var doubleTapWindow: Double { didSet { setDouble("DoubleTapWindow", doubleTapWindow) } }
    var tapMaximumDuration: Double { didSet { setDouble("TapMaximumDuration", tapMaximumDuration) } }
    var tapTravelTolerance: Double { didSet { setDouble("TapTravelTolerance", tapTravelTolerance) } }
    var rapidTapWindow: Double { didSet { setDouble("RapidTapWindow", rapidTapWindow) } }
    var rapidTapActivationCount: Int { didSet { setInt("RapidTapActivationCount", rapidTapActivationCount) } }
    var fireReleaseDelay: Double { didSet { setDouble("FireReleaseDelay", fireReleaseDelay) } }
    var automaticFireInterval: Double { didSet { setDouble("AutomaticFireInterval", automaticFireInterval) } }
    var dynamicCrosshairEnabled: Bool { didSet { setBool("DynamicCrosshairEnabled", dynamicCrosshairEnabled) } }
    var dynamicCrosshairSize: Double { didSet { setDouble("DynamicCrosshairSize", dynamicCrosshairSize) } }
    var dynamicCrosshairOpacity: Double {
        didSet { setDouble("DynamicCrosshairOpacity", dynamicCrosshairOpacity) }
    }
    var dynamicCrosshairType: DynamicCrosshairType {
        didSet { setInt("DynamicCrosshairType", dynamicCrosshairType.rawValue) }
    }
    var dynamicCrosshairAnimation: DynamicCrosshairAnimation {
        didSet { setInt("DynamicCrosshairAnimation", dynamicCrosshairAnimation.rawValue) }
    }
    var leftAimButton: VirtualPadActionButton { didSet { setInt("LeftAimButton", leftAimButton.rawValue) } }
    var leftFireButton: VirtualPadActionButton { didSet { setInt("LeftFireButton", leftFireButton.rawValue) } }
    var leftHoldFireButton: VirtualPadActionButton { didSet { setInt("LeftHoldFireButton", leftHoldFireButton.rawValue) } }
    var rightAimButton: VirtualPadActionButton { didSet { setInt("AimButton", rightAimButton.rawValue) } }
    var rightFireButton: VirtualPadActionButton { didSet { setInt("FireButton", rightFireButton.rawValue) } }
    var rightHoldFireButton: VirtualPadActionButton { didSet { setInt("HoldFireButton", rightHoldFireButton.rawValue) } }

    private init() {
        legacyThumbsticks = Self.bool("LegacyThumbsticks", default: true)
        dynamicThumbsticks = Self.bool("DynamicThumbsticks", default: false)
        swipeCamera = Self.bool("SwipeCamera", default: false)
        gyroscopeCamera = Self.bool("GyroscopeCamera", default: false)
        movementSensitivity = Self.double("MovementSensitivity", default: 1.0)
        lookSensitivity = Self.double("LookSensitivity", default: 1.0)
        let storedSwipeSensitivity = Self.double("SwipeSensitivity", default: 0.28)
        swipeSensitivity = storedSwipeSensitivity
        swipeHorizontalSensitivity = Self.double("SwipeHorizontalSensitivity", default: 1)
        swipeVerticalSensitivity = Self.double("SwipeVerticalSensitivity", default: 1)
        swipeSensitivityWhileAimingEnabled = Self.bool("SwipeSensitivityWhileAimingEnabled", default: true)
        swipeSensitivityWhileAiming = Self.double("SwipeSensitivityWhileAiming", default: 0.28)
        swipeHorizontalSensitivityWhileAiming = Self.double("SwipeHorizontalSensitivityWhileAiming", default: 1)
        swipeVerticalSensitivityWhileAiming = Self.double("SwipeVerticalSensitivityWhileAiming", default: 1)
        swipeSensitivityWhileNotAimingEnabled = Self.bool("SwipeSensitivityWhileNotAimingEnabled", default: true)
        swipeSensitivityWhileNotAiming = Self.double("SwipeSensitivityWhileNotAiming", default: 0.46)
        swipeHorizontalSensitivityWhileNotAiming = Self.double("SwipeHorizontalSensitivityWhileNotAiming", default: 1)
        swipeVerticalSensitivityWhileNotAiming = Self.double("SwipeVerticalSensitivityWhileNotAiming", default: 1)
        gyroSensitivity = Self.double("GyroSensitivity", default: 1.5)
        gyroAcceleration = Self.double("GyroAcceleration", default: 0.35)
        gyroSmoothing = Self.double("GyroSmoothing", default: 0.72)
        gyroDeadZone = Self.double("GyroDeadZone", default: 0.03)
        gyroMaximumRate = Self.double("GyroMaximumRate", default: 6.0)
        invertGyroHorizontal = Self.bool("InvertGyroHorizontal", default: false)
        invertGyroVertical = Self.bool("InvertGyroVertical", default: false)
        thumbstickRadius = Self.double("ThumbstickRadius", default: 52)
        deadZone = Self.double("DeadZone", default: 0.08)
        thumbstickOpacity = Self.double("ThumbstickOpacity", default: 0.72)
        baseOpacity = Self.double("BaseOpacity", default: 0.20)
        trailOpacity = Self.double("TrailOpacity", default: 0.10)
        activationHaptics = Self.bool("ActivationHaptics", default: true)
        leftThumbstickActionsEnabled = Self.bool("LeftThumbstickActionsEnabled", default: false)
        rightThumbstickActionsEnabled = Self.bool("DynamicThumbstickActionsEnabled", default: false)
        holdAimWhileSwipe = Self.bool("HoldAimWhileSwipe", default: false)
        doubleTapToHoldAim = Self.bool("DoubleTapToHoldAim", default: false)
        tapToFire = Self.bool("TapToFire", default: true)
        rapidTapFireEnabled = Self.bool("RapidTapFireEnabled", default: true)
        releaseFireWhenTouchEnds = Self.bool("ReleaseFireWhenTouchEnds", default: true)
        extendFireWhileDragging = Self.bool("ExtendFireWhileDragging", default: true)
        aimReleaseDelay = Self.double("AimReleaseDelay", default: 1.25)
        doubleTapWindow = Self.double("DoubleTapWindow", default: 0.28)
        tapMaximumDuration = Self.double("TapMaximumDuration", default: 0.18)
        tapTravelTolerance = Self.double("TapTravelTolerance", default: 12)
        rapidTapWindow = Self.double("RapidTapWindow", default: 0.28)
        rapidTapActivationCount = Self.int("RapidTapActivationCount", default: 2)
        fireReleaseDelay = Self.double("FireReleaseDelay", default: 0)
        automaticFireInterval = Self.double("AutomaticFireInterval", default: 0.12)
        dynamicCrosshairEnabled = Self.bool("DynamicCrosshairEnabled", default: false)
        dynamicCrosshairSize = Self.double("DynamicCrosshairSize", default: 32)
        dynamicCrosshairOpacity = Self.double("DynamicCrosshairOpacity", default: 0.70)
        dynamicCrosshairType = DynamicCrosshairType(
            rawValue: Self.int("DynamicCrosshairType", default: DynamicCrosshairType.fourBoxes.rawValue)
        ) ?? .fourBoxes
        dynamicCrosshairAnimation = DynamicCrosshairAnimation(
            rawValue: Self.int("DynamicCrosshairAnimation", default: DynamicCrosshairAnimation.reactive.rawValue)
        ) ?? .reactive
        leftAimButton = Self.actionButton("LeftAimButton", default: .rightShoulder)
        leftFireButton = Self.actionButton("LeftFireButton", default: .faceLeft)
        leftHoldFireButton = Self.actionButton("LeftHoldFireButton", default: .faceLeft)
        rightAimButton = Self.actionButton("AimButton", default: .rightShoulder)
        rightFireButton = Self.actionButton("FireButton", default: .faceLeft)
        rightHoldFireButton = Self.actionButton("HoldFireButton", default: .faceLeft)

        if legacyThumbsticks == dynamicThumbsticks {
            legacyThumbsticks = true
            dynamicThumbsticks = false
        }
        if holdAimWhileSwipe && doubleTapToHoldAim {
            doubleTapToHoldAim = false
            setBool("DoubleTapToHoldAim", false)
        }
    }

    func setLegacyThumbsticks(_ enabled: Bool) {
        if enabled {
            dynamicThumbsticks = false
            legacyThumbsticks = true
        } else {
            legacyThumbsticks = false
            dynamicThumbsticks = true
        }
    }

    func setDynamicThumbsticks(_ enabled: Bool) {
        if enabled {
            legacyThumbsticks = false
            dynamicThumbsticks = true
        } else {
            dynamicThumbsticks = false
            legacyThumbsticks = true
        }
    }

    func setHoldAimWhileSwipe(_ enabled: Bool) {
        holdAimWhileSwipe = enabled
        if enabled {
            doubleTapToHoldAim = false
        }
    }

    func setDoubleTapToHoldAim(_ enabled: Bool) {
        doubleTapToHoldAim = enabled
        if enabled {
            holdAimWhileSwipe = false
        }
    }

    func restoreDefaults() {
        legacyThumbsticks = true
        dynamicThumbsticks = false
        swipeCamera = false
        gyroscopeCamera = false
        movementSensitivity = 1
        lookSensitivity = 1
        swipeSensitivity = 0.28
        swipeHorizontalSensitivity = 1
        swipeVerticalSensitivity = 1
        swipeSensitivityWhileAimingEnabled = true
        swipeSensitivityWhileAiming = 0.28
        swipeHorizontalSensitivityWhileAiming = 1
        swipeVerticalSensitivityWhileAiming = 1
        swipeSensitivityWhileNotAimingEnabled = true
        swipeSensitivityWhileNotAiming = 0.46
        swipeHorizontalSensitivityWhileNotAiming = 1
        swipeVerticalSensitivityWhileNotAiming = 1
        gyroSensitivity = 1.5
        gyroAcceleration = 0.35
        gyroSmoothing = 0.72
        gyroDeadZone = 0.03
        gyroMaximumRate = 6
        invertGyroHorizontal = false
        invertGyroVertical = false
        thumbstickRadius = 52
        deadZone = 0.08
        thumbstickOpacity = 0.72
        baseOpacity = 0.20
        trailOpacity = 0.10
        activationHaptics = true
        leftThumbstickActionsEnabled = false
        rightThumbstickActionsEnabled = false
        holdAimWhileSwipe = false
        doubleTapToHoldAim = false
        tapToFire = true
        rapidTapFireEnabled = true
        releaseFireWhenTouchEnds = true
        extendFireWhileDragging = true
        aimReleaseDelay = 1.25
        doubleTapWindow = 0.28
        tapMaximumDuration = 0.18
        tapTravelTolerance = 12
        rapidTapWindow = 0.28
        rapidTapActivationCount = 2
        fireReleaseDelay = 0
        automaticFireInterval = 0.12
        dynamicCrosshairEnabled = false
        dynamicCrosshairSize = 32
        dynamicCrosshairOpacity = 0.70
        dynamicCrosshairType = .fourBoxes
        dynamicCrosshairAnimation = .reactive
        leftAimButton = .rightShoulder
        leftFireButton = .faceLeft
        leftHoldFireButton = .faceLeft
        rightAimButton = .rightShoulder
        rightFireButton = .faceLeft
        rightHoldFireButton = .faceLeft
    }

    func aimButton(for side: VirtualPadThumbstickSide) -> VirtualPadActionButton {
        side == .left ? leftAimButton : rightAimButton
    }

    func fireButton(for side: VirtualPadThumbstickSide) -> VirtualPadActionButton {
        side == .left ? leftFireButton : rightFireButton
    }

    func holdFireButton(for side: VirtualPadThumbstickSide) -> VirtualPadActionButton {
        side == .left ? leftHoldFireButton : rightHoldFireButton
    }

    func effectiveSwipeSensitivity(isAiming: Bool) -> (horizontal: Double, vertical: Double) {
        if isAiming, swipeSensitivityWhileAimingEnabled {
            return (
                swipeSensitivityWhileAiming * swipeHorizontalSensitivityWhileAiming,
                swipeSensitivityWhileAiming * swipeVerticalSensitivityWhileAiming
            )
        }
        if !isAiming, swipeSensitivityWhileNotAimingEnabled {
            return (
                swipeSensitivityWhileNotAiming * swipeHorizontalSensitivityWhileNotAiming,
                swipeSensitivityWhileNotAiming * swipeVerticalSensitivityWhileNotAiming
            )
        }
        return (
            swipeSensitivity * swipeHorizontalSensitivity,
            swipeSensitivity * swipeVerticalSensitivity
        )
    }

    private static func bool(_ key: String, default defaultValue: Bool) -> Bool {
        ARMSX2Bridge.getINIBool(section, key: key, defaultValue: defaultValue)
    }

    private static func double(_ key: String, default defaultValue: Double) -> Double {
        Double(ARMSX2Bridge.getINIFloat(section, key: key, defaultValue: Float(defaultValue)))
    }

    private static func int(_ key: String, default defaultValue: Int) -> Int {
        Int(ARMSX2Bridge.getINIInt(section, key: key, defaultValue: Int32(defaultValue)))
    }

    private static func actionButton(_ key: String, default defaultValue: VirtualPadActionButton) -> VirtualPadActionButton {
        VirtualPadActionButton(rawValue: int(key, default: defaultValue.rawValue)) ?? defaultValue
    }

    private func setBool(_ key: String, _ value: Bool) {
        ARMSX2Bridge.setINIBool(Self.section, key: key, value: value)
    }

    private func setDouble(_ key: String, _ value: Double) {
        ARMSX2Bridge.setINIFloat(Self.section, key: key, value: Float(value))
    }

    private func setInt(_ key: String, _ value: Int) {
        ARMSX2Bridge.setINIInt(Self.section, key: key, value: Int32(value))
    }
}
