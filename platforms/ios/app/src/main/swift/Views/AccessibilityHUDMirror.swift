// AccessibilityHUDMirror.swift — VoiceOver-only overlay exposing HUD stats
// (battery/thermal/RAM) as a distinct element from the game display image.
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct AccessibilityHUDMirror: View {
    @State private var battery: Int = -1
    @State private var thermalState: String = "Nominal"
    @State private var ramGB: Double = 0.0
    @State private var lowPower: Bool = false
    private let timer = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    var body: some View {
        Color.clear
            .frame(width: 0, height: 0)
            .accessibilityElement(children: .ignore)
            .accessibilityLabel(accessibilityLabel)
            .accessibilityAddTraits(.updatesFrequently)
            .onReceive(timer) { _ in refresh() }
    }

    private var accessibilityLabel: String {
        var parts: [String] = []
        if battery >= 0 { parts.append("Battery \(battery)%") }
        parts.append("Thermal \(thermalState)")
        parts.append("RAM \(String(format: "%.1f", ramGB)) GB")
        if lowPower { parts.append("Low Power") }
        return "Status: " + parts.joined(separator: ", ")
    }

    private func refresh() {
        let stats = ARMSX2Bridge.deviceStatsForAccessibility()
        battery = (stats["battery"] as? NSNumber)?.intValue ?? -1
        thermalState = (stats["thermalState"] as? String) ?? "Nominal"
        ramGB = (stats["ramGB"] as? NSNumber)?.doubleValue ?? 0.0
        lowPower = (stats["lowPower"] as? NSNumber)?.boolValue ?? false
    }
}
