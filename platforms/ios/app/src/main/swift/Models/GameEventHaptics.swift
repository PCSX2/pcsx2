// GameEventHaptics.swift — device-level haptic fallback for game rumble events
// when no rumble-capable controller is connected.
// SPDX-License-Identifier: GPL-3.0+

import UIKit

@MainActor
final class GameEventHaptics {
    static let shared = GameEventHaptics()
    private var heavyGenerator: UIImpactFeedbackGenerator?
    private var mediumGenerator: UIImpactFeedbackGenerator?
    private var lastFire = Date.distantPast
    private var hapticsEnabled = true
    private var releasedForEmulationOnlyMode = false

    private init() {
        hapticsEnabled = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "HapticFeedback", defaultValue: true)
    }

    /// Called from the bridge (game rumble path) when no rumble-capable controller is connected.
    /// Respects the user's HapticFeedback setting and throttles to avoid motor fatigue.
    func trigger(large: UInt16, small: UInt16) {
        guard hapticsEnabled, !releasedForEmulationOnlyMode else { return }
        guard large > 0 || small > 0 else { return }
        let now = Date()
        // Throttle to ~20 Hz so sustained rumble does not peg the haptic engine.
        guard now.timeIntervalSince(lastFire) > 0.05 else { return }
        lastFire = now

        let intensity = max(Float(large), Float(small)) / Float(UInt16.max)
        // Heavy motor (large) dominates; fall back to medium for small-only rumble.
        let generator: UIImpactFeedbackGenerator
        if large > 0 {
            if let heavyGenerator {
                generator = heavyGenerator
            } else {
                let created = UIImpactFeedbackGenerator(style: .heavy)
                heavyGenerator = created
                generator = created
            }
        } else if let mediumGenerator {
            generator = mediumGenerator
        } else {
            let created = UIImpactFeedbackGenerator(style: .medium)
            mediumGenerator = created
            generator = created
        }
        generator.impactOccurred(intensity: CGFloat(max(0.3, min(1.0, intensity))))
    }

    func prepareForGameplaySession() {
        releasedForEmulationOnlyMode = false
        refreshEnabled()
    }

    func releaseForEmulationOnlyMode() {
        releasedForEmulationOnlyMode = true
        heavyGenerator = nil
        mediumGenerator = nil
        lastFire = .distantPast
    }

    /// Refresh the enabled flag when the user changes the HapticFeedback setting.
    func refreshEnabled() {
        hapticsEnabled = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "HapticFeedback", defaultValue: true)
    }
}
