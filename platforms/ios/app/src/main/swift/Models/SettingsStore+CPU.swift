// SettingsStore+CPU.swift — CPU rounding/clamp labels and pure helpers
// SPDX-License-Identifier: GPL-3.0+

import Foundation

extension SettingsStore {
    /// 0=Nearest 1=Negative 2=Positive 3=Chop (Zero).
    static let roundModeLabels = ["Nearest", "Negative", "Positive", "Chop (Zero)"]
    /// 0=None 1=Normal 2=Extra 3=Full.
    static let eeClampModeLabels = ["None", "Normal", "Extra", "Full"]
    /// 0=None 1=Normal 2=Extra 3=Extra + Sign.
    static let vuClampModeLabels = ["None", "Normal", "Extra", "Extra + Sign"]

    static func clampedRoundMode(_ value: Int) -> Int {
        min(max(value, 0), 3)
    }

    static func clampedClampMode(_ value: Int) -> Int {
        min(max(value, 0), 3)
    }

    static func clampedCycleSkip(_ value: Int) -> Int {
        min(max(value, 0), 3)
    }

    static func clamped(_ value: Int, to range: ClosedRange<Int>) -> Int {
        min(max(value, range.lowerBound), range.upperBound)
    }

    /// Reconstruct the EE clamp level (0–3) from the three FPU overflow booleans.
    static func eeClampModeFromBools(_ overflow: Bool, _ extra: Bool, _ full: Bool) -> Int {
        if full { return 3 }
        if extra { return 2 }
        if overflow { return 1 }
        return 0
    }

    /// Reconstruct a VU clamp level (0–3) from the three VU overflow booleans.
    static func vuClampModeFromBools(_ overflow: Bool, _ extra: Bool, _ sign: Bool) -> Int {
        if sign { return 3 }
        if extra { return 2 }
        if overflow { return 1 }
        return 0
    }
}
