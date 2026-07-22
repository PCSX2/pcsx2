// SettingsStore+FramePacing.swift — frame pacing preset table + new-default migration
// SPDX-License-Identifier: GPL-3.0+

import Foundation

extension SettingsStore {
    /// Per-preset values for the Frame Pacing picker. `applyFramePacingPreset`
    /// writes these through the individual Setting setters.
    struct FramePacingPresetValues: Sendable {
        let vsyncQueueSize: Int
        let audioOutputLatencyMs: Int
        let audioBufferMs: Int
        let targetFPS: Int
        let syncToHostRefresh: Bool
        let frameLimiterEnabled: Bool
    }

    static let framePacingPresetTable: [FramePacingPreset: FramePacingPresetValues] = [
        .optimal:      .init(vsyncQueueSize: 4, audioOutputLatencyMs: 15, audioBufferMs:  50, targetFPS: 60, syncToHostRefresh: false, frameLimiterEnabled: true),
        .smooth:       .init(vsyncQueueSize: 8, audioOutputLatencyMs: 20, audioBufferMs:  75, targetFPS: 60, syncToHostRefresh: false, frameLimiterEnabled: true),
        .lowLatency:   .init(vsyncQueueSize: 2, audioOutputLatencyMs: 10, audioBufferMs:  30, targetFPS: 60, syncToHostRefresh: false, frameLimiterEnabled: true),
        .batterySaver: .init(vsyncQueueSize: 8, audioOutputLatencyMs: 30, audioBufferMs: 100, targetFPS: 45, syncToHostRefresh: false, frameLimiterEnabled: true),
    ]

    /// One-shot migration to the Optimal default. If the six pacing keys still
    /// match the PCSX2 v1.0 defaults, apply Optimal; otherwise keep the user's
    /// values and mark the preset Custom. Runs once per install.
    ///
    /// Called from SettingsStore.init(), so it must not touch SettingsStore.shared
    /// (that re-enters the in-flight swift_once and deadlocks dispatch_once). It
    /// writes the INI directly; init reads the values back afterward.
    static func migrateFramePacingOptimalDefaultV1() {
        let migrated = ARMSX2Bridge.getINIBool("ARMSX2iOS/Migrations", key: "FramePacingOptimalDefaultV1", defaultValue: false)
        if migrated { return }

        // The v1.0 defaults double as the smart-detect comparator.
        let vsyncQueueSize = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "VsyncQueueSize", defaultValue: 8))
        let outputLatencyMs = Int(ARMSX2Bridge.getINIInt("SPU2/Output", key: "OutputLatencyMS", defaultValue: 20))
        let bufferMs = Int(ARMSX2Bridge.getINIInt("SPU2/Output", key: "BufferMS", defaultValue: 50))
        let nominalScalar = ARMSX2Bridge.getINIFloat("Framerate", key: "NominalScalar", defaultValue: 1.0)
        let syncToHostRefresh = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "SyncToHostRefreshRate", defaultValue: false)

        // Derive frameLimiterEnabled + targetFPS from NominalScalar the same
        // way init does: 1.0 maps to the limiter on at 60 fps on the 59.94 Hz
        // NTSC base; any other scalar counts as customized.
        let limiterEnabled = (nominalScalar > 0.05 && nominalScalar < 10.0)
        let targetFPS = Int((nominalScalar * 59.94).rounded())

        let allDefaults =
            vsyncQueueSize == 8 &&
            outputLatencyMs == 20 &&
            bufferMs == 50 &&
            syncToHostRefresh == false &&
            limiterEnabled == true &&
            targetFPS == 60

        if allDefaults {
            ARMSX2Bridge.setINIInt("ARMSX2iOS/FramePacing", key: "Preset", value: Int32(FramePacingPreset.optimal.rawValue))
            NSLog("[ARMSX2 iOS Settings] Frame Pacing migration: applying Optimal preset")
            // Write the Optimal table straight to the INI. SettingsStore.init
            // reads these keys back next, so they become authoritative. This
            // mirrors applyFramePacingPreset(.optimal) but cannot call it —
            // that would go through SettingsStore.shared (see above).
            ARMSX2Bridge.setINIInt("EmuCore/GS", key: "VsyncQueueSize", value: 4)
            ARMSX2Bridge.setINIInt("SPU2/Output", key: "OutputLatencyMS", value: 15)
            ARMSX2Bridge.setINIInt("SPU2/Output", key: "BufferMS", value: 50)
            ARMSX2Bridge.setINIBool("EmuCore/GS", key: "SyncToHostRefreshRate", value: false)
            ARMSX2Bridge.setINIFloat("Framerate", key: "NominalScalar", value: 1.0)
        } else {
            ARMSX2Bridge.setINIInt("ARMSX2iOS/FramePacing", key: "Preset", value: Int32(FramePacingPreset.custom.rawValue))
            NSLog("[ARMSX2 iOS Settings] Frame Pacing migration: preserving user values as Custom")
        }

        ARMSX2Bridge.setINIBool("ARMSX2iOS/Migrations", key: "FramePacingOptimalDefaultV1", value: true)
    }
}
