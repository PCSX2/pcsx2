// SettingsStore.swift — INI-backed settings for SwiftUI
// SPDX-License-Identifier: GPL-3.0+

import Foundation
import SwiftUI

/// [P51] OSD preset levels
enum OsdPreset: Int, CaseIterable {
    case off = 0
    case simple = 1    // FPS + speed + CPU usage + device stats
    case detail = 2    // All except frame times graph
    case full = 3      // Everything
    case custom = 4    // User-defined toggle set (not derived from a preset table)

    var label: String {
        switch self {
        case .off: return "OFF"
        case .simple: return "Simple"
        case .detail: return "Detail"
        case .full: return "Full"
        case .custom: return "Custom"
        }
    }
}

/// Frame Pacing presets. Mirrors the OsdPreset shape so the consolidated
/// Settings panel and the per-game tab can drive a single picker that fans
/// out to the underlying EmuCore/GS + SPU2/Output + Framerate keys.
enum FramePacingPreset: Int, CaseIterable, Identifiable {
    case optimal = 0       // ARMSX2-tuned default for fresh installs
    case smooth = 1        // larger queues / buffers for visual stability
    case lowLatency = 2    // tight queues + low audio latency for input feel
    case batterySaver = 3  // 45 fps cap + larger audio buffer
    case custom = 4        // user-tweaked; not derived from the table

    var id: Int { rawValue }

    var label: String {
        switch self {
        case .optimal: return "Optimal"
        case .smooth: return "Smooth"
        case .lowLatency: return "Low Latency"
        case .batterySaver: return "Battery Saver"
        case .custom: return "Custom"
        }
    }
}

enum JITScriptProtocol: String, CaseIterable, Identifiable {
    case universal
    case legacy

    var id: String { rawValue }

    var label: String {
        switch self {
        case .universal:
            return "Universal"
        case .legacy:
            return "Legacy"
        }
    }

    var subtitle: String {
        switch self {
        case .universal:
            return "Uses brk #0xf00d prepare + detach."
        case .legacy:
            return "Uses the iOS 17/18 scriptless/legacy JIT path."
        }
    }

    static var defaultValue: JITScriptProtocol {
        ProcessInfo.processInfo.operatingSystemVersion.majorVersion >= 26 ? .universal : .legacy
    }

    static func normalized(_ rawValue: String) -> JITScriptProtocol {
        switch rawValue.lowercased() {
        case "legacy", "utm-dolphin", "utm_dolphin":
            return .legacy
        default:
            return .universal
        }
    }
}

/// A manual per-fix toggle under EmuCore/Gamefixes. The `key` is the exact PCSX2
/// config key; `label` is the localized user-facing name.
struct GameFixOption: Identifiable, Hashable {
    let key: String
    let label: String
    var id: String { key }
}

/// Which analog stick an axis-inversion setting applies to.
enum StickSide: String, CaseIterable, Identifiable {
    case left, right
    var id: String { rawValue }
}

@MainActor
@Observable
final class SettingsStore {
    static let shared = SettingsStore()
    static let minTargetFPS: Float = 15.0
    static let maxTargetFPS: Float = 120.0
    static let defaultTargetFPS: Float = 60.0
    static let minFastForwardScalar: Float = 1.25
    static let maxFastForwardScalar: Float = 10.0
    static let defaultFastForwardScalar: Float = 2.0
    static let defaultEmulatorVolumePercent = 100
    static let textureOffsetRange = -4096...4096
    static let skipDrawRange = 0...5000
    static let defaultOsdPerformancePosition = 3

    /// Manual EmuCore/Gamefixes toggles — see SettingsStore+GameFixes.swift.

    @ObservationIgnored private var suppressINIWrites = false
    @ObservationIgnored private var isProgrammaticOsdFlagChange = false
    @ObservationIgnored private var isAutoMarkingCustom = false
    @ObservationIgnored private var isProgrammaticFramePacingFlagChange = false
    @ObservationIgnored private var isAutoMarkingFramePacingCustom = false
    @ObservationIgnored private var frameLimiterDisabledForFastForward = false
    @ObservationIgnored private var graphicsApplyWorkItem: DispatchWorkItem?
    @ObservationIgnored private var visualSliderDragCount = 0

    /// Coalesces live applies of visual settings so rapid changes reload GS settings
    /// at most once per short window. It is a no-op while a visual slider is being
    /// dragged; the slider's editing-ended handler triggers the apply on release so a
    /// drag does not fire one apply per tick.
    func requestGraphicsApply() {
        guard visualSliderDragCount == 0 else { return }
        graphicsApplyWorkItem?.cancel()
        let workItem = DispatchWorkItem { ARMSX2Bridge.applyGraphicsSettingsNow() }
        graphicsApplyWorkItem = workItem
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.12, execute: workItem)
    }

    /// Used by the graphics `Setting<T>` onSet closures. Swift skips property
    /// observers during init, so they don't fire while loading from the INI;
    /// this no-ops while `suppressINIWrites` is true as a guard against that
    /// ever changing.
    func requestGraphicsApplyGuarded() {
        guard !suppressINIWrites else { return }
        requestGraphicsApply()
    }

    /// Marks the start of a visual slider drag so per-tick value changes do not each
    /// trigger a graphics reload. Balanced by endVisualSliderEdit(), which fires a
    /// single coalesced apply when the last drag ends.
    func beginVisualSliderEdit() {
        visualSliderDragCount += 1
    }

    func endVisualSliderEdit() {
        if visualSliderDragCount > 0 { visualSliderDragCount -= 1 }
        if visualSliderDragCount == 0 { requestGraphicsApply() }
    }

    // ── Emulator / CPU ──
    // writes CoreType + UseArm64Dynarec
    var eeCoreType: Int {
        didSet {
            guard !suppressINIWrites else { return }
            ARMSX2Bridge.setINIInt("EmuCore/CPU", key: "CoreType", value: Int32(eeCoreType))
            ARMSX2Bridge.setINIBool("EmuCore/CPU", key: "UseArm64Dynarec", value: eeCoreType == 2)
        }
    }
    let _iopRecompilerConfig = Setting<Bool>(
        section: "EmuCore/CPU/Recompiler", key: "EnableIOP", default: true,
        writer: ARMSX2Bridge.setINIBool)
    var iopRecompiler: Bool = true { didSet {
        guard !(_iopRecompilerConfig.suppressible && suppressINIWrites) else { return }
        _iopRecompilerConfig.writer(_iopRecompilerConfig.section, _iopRecompilerConfig.key, iopRecompiler)
        _iopRecompilerConfig.onSet?(iopRecompiler)
    }}
    let _vu0RecompilerConfig = Setting<Bool>(
        section: "EmuCore/CPU/Recompiler", key: "EnableVU0", default: true,
        writer: ARMSX2Bridge.setINIBool)
    var vu0Recompiler: Bool = true { didSet {
        guard !(_vu0RecompilerConfig.suppressible && suppressINIWrites) else { return }
        _vu0RecompilerConfig.writer(_vu0RecompilerConfig.section, _vu0RecompilerConfig.key, vu0Recompiler)
        _vu0RecompilerConfig.onSet?(vu0Recompiler)
    }}
    let _vu1RecompilerConfig = Setting<Bool>(
        section: "EmuCore/CPU/Recompiler", key: "EnableVU1", default: true,
        writer: ARMSX2Bridge.setINIBool)
    var vu1Recompiler: Bool = true { didSet {
        guard !(_vu1RecompilerConfig.suppressible && suppressINIWrites) else { return }
        _vu1RecompilerConfig.writer(_vu1RecompilerConfig.section, _vu1RecompilerConfig.key, vu1Recompiler)
        _vu1RecompilerConfig.onSet?(vu1Recompiler)
    }}
    // writes GameISO/FastBoot + EmuCore/EnableFastBoot
    var fastBoot: Bool {
        didSet {
            guard !suppressINIWrites else { return }
            ARMSX2Bridge.setINIBool("GameISO", key: "FastBoot", value: fastBoot)
            ARMSX2Bridge.setINIBool("EmuCore", key: "EnableFastBoot", value: fastBoot)
        }
    }
    // writes ManualFastmem + EnableFastmem
    var fastmem: Bool {
        didSet {
            guard !suppressINIWrites else { return }
            ARMSX2Bridge.setINIBool("ARMSX2iOS/Speedhacks", key: "ManualFastmem", value: true)
            ARMSX2Bridge.setINIBool("EmuCore/CPU/Recompiler", key: "EnableFastmem", value: fastmem)
        }
    }

    // ── CPU Rounding & Clamping ──
    // FPU/VU rounding and clamping improve accuracy/compatibility for specific games.
    // Clamp modes are stored as a single 0–3 level and unpacked to the three
    // (EE) / six (VU0+VU1) boolean keys the PCSX2 recompiler reads, matching the
    // Android refresh UI and the upstream PCSX2 GUI. Changes take effect on next boot.
    let _eeFpuRoundModeConfig = Setting<Int>(
        section: "EmuCore/CPU", key: "FPU.Roundmode", default: 3,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clampedRoundMode(v))) })
    var eeFpuRoundMode: Int = 3 { didSet {
        guard !(_eeFpuRoundModeConfig.suppressible && suppressINIWrites) else { return }
        _eeFpuRoundModeConfig.writer(_eeFpuRoundModeConfig.section, _eeFpuRoundModeConfig.key, eeFpuRoundMode)
        _eeFpuRoundModeConfig.onSet?(eeFpuRoundMode)
    }}
    let _vu0RoundModeConfig = Setting<Int>(
        section: "EmuCore/CPU", key: "VU0.Roundmode", default: 3,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clampedRoundMode(v))) })
    var vu0RoundMode: Int = 3 { didSet {
        guard !(_vu0RoundModeConfig.suppressible && suppressINIWrites) else { return }
        _vu0RoundModeConfig.writer(_vu0RoundModeConfig.section, _vu0RoundModeConfig.key, vu0RoundMode)
        _vu0RoundModeConfig.onSet?(vu0RoundMode)
    }}
    let _vu1RoundModeConfig = Setting<Int>(
        section: "EmuCore/CPU", key: "VU1.Roundmode", default: 3,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clampedRoundMode(v))) })
    var vu1RoundMode: Int = 3 { didSet {
        guard !(_vu1RoundModeConfig.suppressible && suppressINIWrites) else { return }
        _vu1RoundModeConfig.writer(_vu1RoundModeConfig.section, _vu1RoundModeConfig.key, vu1RoundMode)
        _vu1RoundModeConfig.onSet?(vu1RoundMode)
    }}
    var eeClampMode: Int {
        didSet {
            guard !suppressINIWrites else { return }
            Self.applyEEClampMode(Self.clampedClampMode(eeClampMode))
        }
    }
    var vuClampMode: Int {
        didSet {
            guard !suppressINIWrites else { return }
            Self.applyVUClampMode(Self.clampedClampMode(vuClampMode))
        }
    }
    var frameLimiterEnabled: Bool {
        didSet {
            applyFrameLimiterSettings()
            markFramePacingCustom()
        }
    }
    var fastForwardRuntimeEnabled = false
    // clamps to 15...120
    var targetFPS: Float {
        didSet {
            let normalized = Self.clampedTargetFPS(targetFPS)
            guard abs(targetFPS - normalized) <= 0.001 else {
                targetFPS = normalized
                return
            }
            applyFrameLimiterSettings()
            markFramePacingCustom()
        }
    }
    // clamps to 1.25...10.0
    var fastForwardScalar: Float {
        didSet {
            let normalized = Self.clampedSpeedScalar(fastForwardScalar)
            guard abs(fastForwardScalar - normalized) <= 0.001 else {
                fastForwardScalar = normalized
                return
            }
            guard !suppressINIWrites else { return }
            ARMSX2Bridge.setINIFloat("Framerate", key: "TurboScalar", value: fastForwardScalar)
        }
    }
    // clamps to 0...150
    var emulatorVolumePercent: Int {
        didSet {
            let normalized = Self.clampedEmulatorVolumePercent(emulatorVolumePercent)
            guard emulatorVolumePercent == normalized else {
                emulatorVolumePercent = normalized
                return
            }
            guard !suppressINIWrites else { return }
            ARMSX2Bridge.setEmulatorVolumePercent(Int32(normalized))
        }
    }

    // ── Audio Output (SPU2/Output) ── applied live by the SPU2 stream.
    let _audioTimeStretchConfig = Setting<Bool>(
        section: "SPU2/Output", key: "SyncMode", default: true,
        writer: { s, k, v in ARMSX2Bridge.setINIString(s, key: k, value: v ? "TimeStretch" : "Disabled") })
    var audioTimeStretch: Bool = true { didSet {
        guard !(_audioTimeStretchConfig.suppressible && suppressINIWrites) else { return }
        _audioTimeStretchConfig.writer(_audioTimeStretchConfig.section, _audioTimeStretchConfig.key, audioTimeStretch)
        _audioTimeStretchConfig.onSet?(audioTimeStretch)
    }}
    let _audioBufferMsConfig = Setting<Int>(
        section: "SPU2/Output", key: "BufferMS", default: 50,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 10...200))) })
    var audioBufferMs: Int = 50 { didSet {
        guard !(_audioBufferMsConfig.suppressible && suppressINIWrites) else { return }
        _audioBufferMsConfig.writer(_audioBufferMsConfig.section, _audioBufferMsConfig.key, audioBufferMs)
        _audioBufferMsConfig.onSet?(audioBufferMs)
        markFramePacingCustom()
    }}
    let _audioOutputLatencyMsConfig = Setting<Int>(
        section: "SPU2/Output", key: "OutputLatencyMS", default: 20,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 5...200))) })
    var audioOutputLatencyMs: Int = 20 { didSet {
        guard !(_audioOutputLatencyMsConfig.suppressible && suppressINIWrites) else { return }
        _audioOutputLatencyMsConfig.writer(_audioOutputLatencyMsConfig.section, _audioOutputLatencyMsConfig.key, audioOutputLatencyMs)
        _audioOutputLatencyMsConfig.onSet?(audioOutputLatencyMs)
        markFramePacingCustom()
    }}
    let _audioFastForwardVolumeConfig = Setting<Int>(
        section: "SPU2/Output", key: "FastForwardVolume", default: 100,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 0...200))) })
    var audioFastForwardVolume: Int = 100 { didSet {
        guard !(_audioFastForwardVolumeConfig.suppressible && suppressINIWrites) else { return }
        _audioFastForwardVolumeConfig.writer(_audioFastForwardVolumeConfig.section, _audioFastForwardVolumeConfig.key, audioFastForwardVolume)
        _audioFastForwardVolumeConfig.onSet?(audioFastForwardVolume)
    }}
    let _audioSwapChannelsConfig = Setting<Bool>(
        section: "SPU2/Output", key: "SwapChannels", default: false,
        writer: ARMSX2Bridge.setINIBool)
    var audioSwapChannels: Bool = false { didSet {
        guard !(_audioSwapChannelsConfig.suppressible && suppressINIWrites) else { return }
        _audioSwapChannelsConfig.writer(_audioSwapChannelsConfig.section, _audioSwapChannelsConfig.key, audioSwapChannels)
        _audioSwapChannelsConfig.onSet?(audioSwapChannels)
    }}
    var ntscFramerate: Float {
        didSet {
            guard !suppressINIWrites else { return }
            ARMSX2Bridge.setINIFloat("EmuCore/GS", key: "FramerateNTSC", value: ntscFramerate)
            applyFrameLimiterSettings()
        }
    }
    let _palFramerateConfig = Setting<Float>(
        section: "EmuCore/GS", key: "FrameratePAL", default: 50.0,
        writer: ARMSX2Bridge.setINIFloat)
    var palFramerate: Float = 50.0 { didSet {
        guard !(_palFramerateConfig.suppressible && suppressINIWrites) else { return }
        _palFramerateConfig.writer(_palFramerateConfig.section, _palFramerateConfig.key, palFramerate)
        _palFramerateConfig.onSet?(palFramerate)
    }}

    // ── Boot ──
    let _fastCDVDConfig = Setting<Bool>(
        section: "EmuCore/Speedhacks", key: "fastCDVD", default: false,
        writer: ARMSX2Bridge.setINIBool)
    var fastCDVD: Bool = false { didSet {
        guard !(_fastCDVDConfig.suppressible && suppressINIWrites) else { return }
        _fastCDVDConfig.writer(_fastCDVDConfig.section, _fastCDVDConfig.key, fastCDVD)
        _fastCDVDConfig.onSet?(fastCDVD)
    }}

    // ── Advanced Speedhacks ──
    let _eeCycleRateConfig = Setting<Int>(
        section: "EmuCore/Speedhacks", key: "EECycleRate", default: 0,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) })
    var eeCycleRate: Int = 0 { didSet {
        guard !(_eeCycleRateConfig.suppressible && suppressINIWrites) else { return }
        _eeCycleRateConfig.writer(_eeCycleRateConfig.section, _eeCycleRateConfig.key, eeCycleRate)
        _eeCycleRateConfig.onSet?(eeCycleRate)
    }}
    let _vu1InstantConfig = Setting<Bool>(
        section: "EmuCore/Speedhacks", key: "vu1Instant", default: true,
        writer: ARMSX2Bridge.setINIBool)
    var vu1Instant: Bool = true { didSet {
        guard !(_vu1InstantConfig.suppressible && suppressINIWrites) else { return }
        _vu1InstantConfig.writer(_vu1InstantConfig.section, _vu1InstantConfig.key, vu1Instant)
        _vu1InstantConfig.onSet?(vu1Instant)
    }}
    // writes ManualMTVU + ManualMTVUVersion + vuThread
    var mtvu: Bool {
        didSet {
            guard !suppressINIWrites else { return }
            ARMSX2Bridge.setINIBool("ARMSX2iOS/Speedhacks", key: "ManualMTVU", value: true)
            ARMSX2Bridge.setINIInt("ARMSX2iOS/Speedhacks", key: "ManualMTVUVersion", value: 3)
            ARMSX2Bridge.setINIBool("EmuCore/Speedhacks", key: "vuThread", value: mtvu)
        }
    }
    let _waitLoopConfig = Setting<Bool>(
        section: "EmuCore/Speedhacks", key: "WaitLoop", default: true,
        writer: ARMSX2Bridge.setINIBool)
    var waitLoop: Bool = true { didSet {
        guard !(_waitLoopConfig.suppressible && suppressINIWrites) else { return }
        _waitLoopConfig.writer(_waitLoopConfig.section, _waitLoopConfig.key, waitLoop)
        _waitLoopConfig.onSet?(waitLoop)
    }}
    let _intcStatConfig = Setting<Bool>(
        section: "EmuCore/Speedhacks", key: "IntcStat", default: true,
        writer: ARMSX2Bridge.setINIBool)
    var intcStat: Bool = true { didSet {
        guard !(_intcStatConfig.suppressible && suppressINIWrites) else { return }
        _intcStatConfig.writer(_intcStatConfig.section, _intcStatConfig.key, intcStat)
        _intcStatConfig.onSet?(intcStat)
    }}
    let _eeCycleSkipConfig = Setting<Int>(
        section: "EmuCore/Speedhacks", key: "EECycleSkip", default: 0,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clampedCycleSkip(v))) })
    var eeCycleSkip: Int = 0 { didSet {
        guard !(_eeCycleSkipConfig.suppressible && suppressINIWrites) else { return }
        _eeCycleSkipConfig.writer(_eeCycleSkipConfig.section, _eeCycleSkipConfig.key, eeCycleSkip)
        _eeCycleSkipConfig.onSet?(eeCycleSkip)
    }}
    let _vuFlagHackConfig = Setting<Bool>(
        section: "EmuCore/Speedhacks", key: "vuFlagHack", default: true,
        writer: ARMSX2Bridge.setINIBool)
    var vuFlagHack: Bool = true { didSet {
        guard !(_vuFlagHackConfig.suppressible && suppressINIWrites) else { return }
        _vuFlagHackConfig.writer(_vuFlagHackConfig.section, _vuFlagHackConfig.key, vuFlagHack)
        _vuFlagHackConfig.onSet?(vuFlagHack)
    }}
    let _enableCheatsConfig = Setting<Bool>(
        section: "EmuCore", key: "EnableCheats", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var enableCheats: Bool = false { didSet {
        guard !(_enableCheatsConfig.suppressible && suppressINIWrites) else { return }
        _enableCheatsConfig.writer(_enableCheatsConfig.section, _enableCheatsConfig.key, enableCheats)
        _enableCheatsConfig.onSet?(enableCheats)
    }}
    let _enablePatchesConfig = Setting<Bool>(
        section: "EmuCore", key: "EnablePatches", default: true,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var enablePatches: Bool = true { didSet {
        guard !(_enablePatchesConfig.suppressible && suppressINIWrites) else { return }
        _enablePatchesConfig.writer(_enablePatchesConfig.section, _enablePatchesConfig.key, enablePatches)
        _enablePatchesConfig.onSet?(enablePatches)
    }}
    let _enableGameFixesConfig = Setting<Bool>(
        section: "EmuCore", key: "EnableGameFixes", default: true,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var enableGameFixes: Bool = true { didSet {
        guard !(_enableGameFixesConfig.suppressible && suppressINIWrites) else { return }
        _enableGameFixesConfig.writer(_enableGameFixesConfig.section, _enableGameFixesConfig.key, enableGameFixes)
        _enableGameFixesConfig.onSet?(enableGameFixes)
    }}
    let _enableGameDBHardwareFixesConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "UserHacks", default: true,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIBool(s, key: k, value: !v) })
    var enableGameDBHardwareFixes: Bool = true { didSet {
        guard !(_enableGameDBHardwareFixesConfig.suppressible && suppressINIWrites) else { return }
        _enableGameDBHardwareFixesConfig.writer(_enableGameDBHardwareFixesConfig.section, _enableGameDBHardwareFixesConfig.key, enableGameDBHardwareFixes)
        _enableGameDBHardwareFixesConfig.onSet?(enableGameDBHardwareFixes)
    }}
    let _enableWidescreenPatchesConfig = Setting<Bool>(
        section: "EmuCore", key: "EnableWideScreenPatches", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var enableWidescreenPatches: Bool = false { didSet {
        guard !(_enableWidescreenPatchesConfig.suppressible && suppressINIWrites) else { return }
        _enableWidescreenPatchesConfig.writer(_enableWidescreenPatchesConfig.section, _enableWidescreenPatchesConfig.key, enableWidescreenPatches)
        _enableWidescreenPatchesConfig.onSet?(enableWidescreenPatches)
    }}
    let _enableNoInterlacingPatchesConfig = Setting<Bool>(
        section: "EmuCore", key: "EnableNoInterlacingPatches", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var enableNoInterlacingPatches: Bool = false { didSet {
        guard !(_enableNoInterlacingPatchesConfig.suppressible && suppressINIWrites) else { return }
        _enableNoInterlacingPatchesConfig.writer(_enableNoInterlacingPatchesConfig.section, _enableNoInterlacingPatchesConfig.key, enableNoInterlacingPatches)
        _enableNoInterlacingPatchesConfig.onSet?(enableNoInterlacingPatches)
    }}
    let _hostFilesystemConfig = Setting<Bool>(
        section: "EmuCore", key: "HostFs", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var hostFilesystem: Bool = false { didSet {
        guard !(_hostFilesystemConfig.suppressible && suppressINIWrites) else { return }
        _hostFilesystemConfig.writer(_hostFilesystemConfig.section, _hostFilesystemConfig.key, hostFilesystem)
        _hostFilesystemConfig.onSet?(hostFilesystem)
    }}

    // ── Manual Game Fixes (EmuCore/Gamefixes/<key>) ──
    // Dictionary-backed because the 17 fixes are homogeneous toggles. Effective only
    // while GameDB Core Fixes (enableGameFixes) is on. Toggling one writes only its
    // own INI key.
    var gameFixes: [String: Bool] = [:]

    func gameFixEnabled(_ key: String) -> Bool {
        gameFixes[key] ?? false
    }

    func setGameFix(_ key: String, _ value: Bool) {
        gameFixes[key] = value
        guard !suppressINIWrites else { return }
        ARMSX2Bridge.setINIBool("EmuCore/Gamefixes", key: key, value: value)
    }

    private static func loadGameFixes() -> [String: Bool] {
        var values: [String: Bool] = [:]
        for option in gameFixOptions {
            values[option.key] = ARMSX2Bridge.getINIBool("EmuCore/Gamefixes", key: option.key, defaultValue: false)
        }
        return values
    }

    // ── Graphics ──

    /// MetalFX Spatial upscaling requires iOS 16+ and a device GPU that supports it.
    /// Probes at runtime so the UI can hide the option on unsupported hardware.
    var isMetalFXAvailable: Bool {
        ARMSX2Bridge.isMetalFXSupported()
    }

    let _rendererConfig = Setting<Int>(
        section: "EmuCore/GS", key: "Renderer", default: 17,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) })
    var renderer: Int = 17 { didSet {
        guard !(_rendererConfig.suppressible && suppressINIWrites) else { return }
        _rendererConfig.writer(_rendererConfig.section, _rendererConfig.key, renderer)
        _rendererConfig.onSet?(renderer)
    }}
    let _upscaleMultiplierConfig = Setting<Float>(
        section: "EmuCore/GS", key: "upscale_multiplier", default: 1.0,
        suppressible: false,
        writer: ARMSX2Bridge.setINIFloat,
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var upscaleMultiplier: Float = 1.0 { didSet {
        guard !(_upscaleMultiplierConfig.suppressible && suppressINIWrites) else { return }
        _upscaleMultiplierConfig.writer(_upscaleMultiplierConfig.section, _upscaleMultiplierConfig.key, upscaleMultiplier)
        _upscaleMultiplierConfig.onSet?(upscaleMultiplier)
    }}
    let _vsyncQueueSizeConfig = Setting<Int>(
        section: "EmuCore/GS", key: "VsyncQueueSize", default: 8,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) })
    var vsyncQueueSize: Int = 8 { didSet {
        guard !(_vsyncQueueSizeConfig.suppressible && suppressINIWrites) else { return }
        _vsyncQueueSizeConfig.writer(_vsyncQueueSizeConfig.section, _vsyncQueueSizeConfig.key, vsyncQueueSize)
        _vsyncQueueSizeConfig.onSet?(vsyncQueueSize)
        markFramePacingCustom()
    }}
    let _textureFilteringConfig = Setting<Int>(
        section: "EmuCore/GS", key: "filter", default: 2,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) },
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var textureFiltering: Int = 2 { didSet {
        guard !(_textureFilteringConfig.suppressible && suppressINIWrites) else { return }
        _textureFilteringConfig.writer(_textureFilteringConfig.section, _textureFilteringConfig.key, textureFiltering)
        _textureFilteringConfig.onSet?(textureFiltering)
    }}
    let _backThreadModeConfig = Setting<Int>(
        section: "EmuCore/GS", key: "GSBackThreadMode", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) },
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var backThreadMode: Int = 0 { didSet {
        guard !(_backThreadModeConfig.suppressible && suppressINIWrites) else { return }
        _backThreadModeConfig.writer(_backThreadModeConfig.section, _backThreadModeConfig.key, backThreadMode)
        _backThreadModeConfig.onSet?(backThreadMode)
    }}
    let _hardwareMipmappingConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "hw_mipmap", default: true,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var hardwareMipmapping: Bool = true { didSet {
        guard !(_hardwareMipmappingConfig.suppressible && suppressINIWrites) else { return }
        _hardwareMipmappingConfig.writer(_hardwareMipmappingConfig.section, _hardwareMipmappingConfig.key, hardwareMipmapping)
        _hardwareMipmappingConfig.onSet?(hardwareMipmapping)
    }}
    let _fxaaConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "fxaa", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool,
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var fxaa: Bool = false { didSet {
        guard !(_fxaaConfig.suppressible && suppressINIWrites) else { return }
        _fxaaConfig.writer(_fxaaConfig.section, _fxaaConfig.key, fxaa)
        _fxaaConfig.onSet?(fxaa)
    }}
    let _casModeConfig = Setting<Int>(
        section: "EmuCore/GS", key: "CASMode", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) },
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var casMode: Int = 0 { didSet {
        guard !(_casModeConfig.suppressible && suppressINIWrites) else { return }
        _casModeConfig.writer(_casModeConfig.section, _casModeConfig.key, casMode)
        _casModeConfig.onSet?(casMode)
    }}
    let _casSharpnessConfig = Setting<Int>(
        section: "EmuCore/GS", key: "CASSharpness", default: 50,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) },
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var casSharpness: Int = 50 { didSet {
        guard !(_casSharpnessConfig.suppressible && suppressINIWrites) else { return }
        _casSharpnessConfig.writer(_casSharpnessConfig.section, _casSharpnessConfig.key, casSharpness)
        _casSharpnessConfig.onSet?(casSharpness)
    }}
    let _interlaceModeConfig = Setting<Int>(
        section: "EmuCore/GS", key: "deinterlace_mode", default: 7,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) },
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var interlaceMode: Int = 7 { didSet {
        guard !(_interlaceModeConfig.suppressible && suppressINIWrites) else { return }
        _interlaceModeConfig.writer(_interlaceModeConfig.section, _interlaceModeConfig.key, interlaceMode)
        _interlaceModeConfig.onSet?(interlaceMode)
    }}
    let _aspectRatioConfig = Setting<Int>(
        section: "EmuCore/GS", key: "AspectRatio", default: 1,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIString(s, key: k, value: SettingsStore.aspectRatioName(for: v)) },
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var aspectRatio: Int = 1 { didSet {
        guard !(_aspectRatioConfig.suppressible && suppressINIWrites) else { return }
        _aspectRatioConfig.writer(_aspectRatioConfig.section, _aspectRatioConfig.key, aspectRatio)
        _aspectRatioConfig.onSet?(aspectRatio)
    }}
    let _blendingAccuracyConfig = Setting<Int>(
        section: "EmuCore/GS", key: "accurate_blending_unit", default: 1,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) })
    var blendingAccuracy: Int = 1 { didSet {
        guard !(_blendingAccuracyConfig.suppressible && suppressINIWrites) else { return }
        _blendingAccuracyConfig.writer(_blendingAccuracyConfig.section, _blendingAccuracyConfig.key, blendingAccuracy)
        _blendingAccuracyConfig.onSet?(blendingAccuracy)
    }}
    let _ditheringConfig = Setting<Int>(
        section: "EmuCore/GS", key: "dithering_ps2", default: 2,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) })
    var dithering: Int = 2 { didSet {
        guard !(_ditheringConfig.suppressible && suppressINIWrites) else { return }
        _ditheringConfig.writer(_ditheringConfig.section, _ditheringConfig.key, dithering)
        _ditheringConfig.onSet?(dithering)
    }}
    let _trilinearFilteringConfig = Setting<Int>(
        section: "EmuCore/GS", key: "TriFilter", default: -1,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) },
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var trilinearFiltering: Int = -1 { didSet {
        guard !(_trilinearFilteringConfig.suppressible && suppressINIWrites) else { return }
        _trilinearFilteringConfig.writer(_trilinearFilteringConfig.section, _trilinearFilteringConfig.key, trilinearFiltering)
        _trilinearFilteringConfig.onSet?(trilinearFiltering)
    }}
    let _halfPixelOffsetConfig = Setting<Int>(
        section: "EmuCore/GS", key: "UserHacks_HalfPixelOffset", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) })
    var halfPixelOffset: Int = 0 { didSet {
        guard !(_halfPixelOffsetConfig.suppressible && suppressINIWrites) else { return }
        _halfPixelOffsetConfig.writer(_halfPixelOffsetConfig.section, _halfPixelOffsetConfig.key, halfPixelOffset)
        _halfPixelOffsetConfig.onSet?(halfPixelOffset)
    }}
    let _roundSpriteConfig = Setting<Int>(
        section: "EmuCore/GS", key: "UserHacks_round_sprite_offset", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) })
    var roundSprite: Int = 0 { didSet {
        guard !(_roundSpriteConfig.suppressible && suppressINIWrites) else { return }
        _roundSpriteConfig.writer(_roundSpriteConfig.section, _roundSpriteConfig.key, roundSprite)
        _roundSpriteConfig.onSet?(roundSprite)
    }}
    let _alignSpriteConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "UserHacks_align_sprite_X", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var alignSprite: Bool = false { didSet {
        guard !(_alignSpriteConfig.suppressible && suppressINIWrites) else { return }
        _alignSpriteConfig.writer(_alignSpriteConfig.section, _alignSpriteConfig.key, alignSprite)
        _alignSpriteConfig.onSet?(alignSprite)
    }}
    let _mergeSpriteConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "UserHacks_merge_pp_sprite", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var mergeSprite: Bool = false { didSet {
        guard !(_mergeSpriteConfig.suppressible && suppressINIWrites) else { return }
        _mergeSpriteConfig.writer(_mergeSpriteConfig.section, _mergeSpriteConfig.key, mergeSprite)
        _mergeSpriteConfig.onSet?(mergeSprite)
    }}
    let _wildArmsOffsetConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "UserHacks_ForceEvenSpritePosition", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var wildArmsOffset: Bool = false { didSet {
        guard !(_wildArmsOffsetConfig.suppressible && suppressINIWrites) else { return }
        _wildArmsOffsetConfig.writer(_wildArmsOffsetConfig.section, _wildArmsOffsetConfig.key, wildArmsOffset)
        _wildArmsOffsetConfig.onSet?(wildArmsOffset)
    }}
    // clamps to -4096...4096
    var textureOffsetX: Int {
        didSet {
            let normalized = Self.clampedTextureOffset(textureOffsetX)
            guard textureOffsetX == normalized else {
                textureOffsetX = normalized
                return
            }
            ARMSX2Bridge.setINIInt("EmuCore/GS", key: "UserHacks_TCOffsetX", value: Int32(textureOffsetX))
        }
    }
    // clamps to -4096...4096
    var textureOffsetY: Int {
        didSet {
            let normalized = Self.clampedTextureOffset(textureOffsetY)
            guard textureOffsetY == normalized else {
                textureOffsetY = normalized
                return
            }
            ARMSX2Bridge.setINIInt("EmuCore/GS", key: "UserHacks_TCOffsetY", value: Int32(textureOffsetY))
        }
    }
    // clamps to 0...5000
    var skipDrawStart: Int {
        didSet {
            let normalized = Self.clampedSkipDraw(skipDrawStart)
            guard skipDrawStart == normalized else {
                skipDrawStart = normalized
                return
            }
            ARMSX2Bridge.setINIInt("EmuCore/GS", key: "UserHacks_SkipDraw_Start", value: Int32(skipDrawStart))
        }
    }
    // clamps to 0...5000
    var skipDrawEnd: Int {
        didSet {
            let normalized = Self.clampedSkipDraw(skipDrawEnd)
            guard skipDrawEnd == normalized else {
                skipDrawEnd = normalized
                return
            }
            ARMSX2Bridge.setINIInt("EmuCore/GS", key: "UserHacks_SkipDraw_End", value: Int32(skipDrawEnd))
        }
    }
    let _loadTextureReplacementsConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "LoadTextureReplacements", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var loadTextureReplacements: Bool = false { didSet {
        guard !(_loadTextureReplacementsConfig.suppressible && suppressINIWrites) else { return }
        _loadTextureReplacementsConfig.writer(_loadTextureReplacementsConfig.section, _loadTextureReplacementsConfig.key, loadTextureReplacements)
        _loadTextureReplacementsConfig.onSet?(loadTextureReplacements)
    }}
    let _loadTextureReplacementsAsyncConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "LoadTextureReplacementsAsync", default: true,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var loadTextureReplacementsAsync: Bool = true { didSet {
        guard !(_loadTextureReplacementsAsyncConfig.suppressible && suppressINIWrites) else { return }
        _loadTextureReplacementsAsyncConfig.writer(_loadTextureReplacementsAsyncConfig.section, _loadTextureReplacementsAsyncConfig.key, loadTextureReplacementsAsync)
        _loadTextureReplacementsAsyncConfig.onSet?(loadTextureReplacementsAsync)
    }}
    let _precacheTextureReplacementsConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "PrecacheTextureReplacements", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var precacheTextureReplacements: Bool = false { didSet {
        guard !(_precacheTextureReplacementsConfig.suppressible && suppressINIWrites) else { return }
        _precacheTextureReplacementsConfig.writer(_precacheTextureReplacementsConfig.section, _precacheTextureReplacementsConfig.key, precacheTextureReplacements)
        _precacheTextureReplacementsConfig.onSet?(precacheTextureReplacements)
    }}
    let _texturePreloadingConfig = Setting<Int>(
        section: "EmuCore/GS", key: "texture_preloading", default: 2,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) })
    var texturePreloading: Int = 2 { didSet {
        guard !(_texturePreloadingConfig.suppressible && suppressINIWrites) else { return }
        _texturePreloadingConfig.writer(_texturePreloadingConfig.section, _texturePreloadingConfig.key, texturePreloading)
        _texturePreloadingConfig.onSet?(texturePreloading)
    }}
    let _dumpReplaceableTexturesConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "DumpReplaceableTextures", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var dumpReplaceableTextures: Bool = false { didSet {
        guard !(_dumpReplaceableTexturesConfig.suppressible && suppressINIWrites) else { return }
        _dumpReplaceableTexturesConfig.writer(_dumpReplaceableTexturesConfig.section, _dumpReplaceableTexturesConfig.key, dumpReplaceableTextures)
        _dumpReplaceableTexturesConfig.onSet?(dumpReplaceableTextures)
    }}
    let _dumpReplaceableMipmapsConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "DumpReplaceableMipmaps", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var dumpReplaceableMipmaps: Bool = false { didSet {
        guard !(_dumpReplaceableMipmapsConfig.suppressible && suppressINIWrites) else { return }
        _dumpReplaceableMipmapsConfig.writer(_dumpReplaceableMipmapsConfig.section, _dumpReplaceableMipmapsConfig.key, dumpReplaceableMipmaps)
        _dumpReplaceableMipmapsConfig.onSet?(dumpReplaceableMipmaps)
    }}
    let _dumpTexturesWithFMVActiveConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "DumpTexturesWithFMVActive", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var dumpTexturesWithFMVActive: Bool = false { didSet {
        guard !(_dumpTexturesWithFMVActiveConfig.suppressible && suppressINIWrites) else { return }
        _dumpTexturesWithFMVActiveConfig.writer(_dumpTexturesWithFMVActiveConfig.section, _dumpTexturesWithFMVActiveConfig.key, dumpTexturesWithFMVActive)
        _dumpTexturesWithFMVActiveConfig.onSet?(dumpTexturesWithFMVActive)
    }}
    let _dumpDirectTexturesConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "DumpDirectTextures", default: true,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var dumpDirectTextures: Bool = true { didSet {
        guard !(_dumpDirectTexturesConfig.suppressible && suppressINIWrites) else { return }
        _dumpDirectTexturesConfig.writer(_dumpDirectTexturesConfig.section, _dumpDirectTexturesConfig.key, dumpDirectTextures)
        _dumpDirectTexturesConfig.onSet?(dumpDirectTextures)
    }}
    let _dumpPaletteTexturesConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "DumpPaletteTextures", default: true,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var dumpPaletteTextures: Bool = true { didSet {
        guard !(_dumpPaletteTexturesConfig.suppressible && suppressINIWrites) else { return }
        _dumpPaletteTexturesConfig.writer(_dumpPaletteTexturesConfig.section, _dumpPaletteTexturesConfig.key, dumpPaletteTextures)
        _dumpPaletteTexturesConfig.onSet?(dumpPaletteTextures)
    }}

    // ── GS Hardware Fixes (EmuCore/GS) ──
    // Compatibility-oriented hardware-renderer fixes. AAT (HWAccurateAlphaTest) and
    // Texture Inside RT close the GameDB advisory gap added in 2.3.2. Applied live by
    // the GS thread (no VM restart); some may require GameDB Graphics Fixes off.
    let _hwAccurateAlphaTestConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "HWAccurateAlphaTest", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var hwAccurateAlphaTest: Bool = false { didSet {
        guard !(_hwAccurateAlphaTestConfig.suppressible && suppressINIWrites) else { return }
        _hwAccurateAlphaTestConfig.writer(_hwAccurateAlphaTestConfig.section, _hwAccurateAlphaTestConfig.key, hwAccurateAlphaTest)
        _hwAccurateAlphaTestConfig.onSet?(hwAccurateAlphaTest)
    }}
    let _textureInsideRtConfig = Setting<Int>(
        section: "EmuCore/GS", key: "UserHacks_TextureInsideRt", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 0...2))) })
    var textureInsideRt: Int = 0 { didSet {
        guard !(_textureInsideRtConfig.suppressible && suppressINIWrites) else { return }
        _textureInsideRtConfig.writer(_textureInsideRtConfig.section, _textureInsideRtConfig.key, textureInsideRt)
        _textureInsideRtConfig.onSet?(textureInsideRt)
    }}
    let _limit24BitDepthConfig = Setting<Int>(
        section: "EmuCore/GS", key: "UserHacks_Limit24BitDepth", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 0...2))) })
    var limit24BitDepth: Int = 0 { didSet {
        guard !(_limit24BitDepthConfig.suppressible && suppressINIWrites) else { return }
        _limit24BitDepthConfig.writer(_limit24BitDepthConfig.section, _limit24BitDepthConfig.key, limit24BitDepth)
        _limit24BitDepthConfig.onSet?(limit24BitDepth)
    }}
    let _nativeScalingConfig = Setting<Int>(
        section: "EmuCore/GS", key: "UserHacks_native_scaling", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 0...4))) })
    var nativeScaling: Int = 0 { didSet {
        guard !(_nativeScalingConfig.suppressible && suppressINIWrites) else { return }
        _nativeScalingConfig.writer(_nativeScalingConfig.section, _nativeScalingConfig.key, nativeScaling)
        _nativeScalingConfig.onSet?(nativeScaling)
    }}
    let _cpuClutRenderConfig = Setting<Int>(
        section: "EmuCore/GS", key: "UserHacks_CPUCLUTRender", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 0...2))) })
    var cpuClutRender: Int = 0 { didSet {
        guard !(_cpuClutRenderConfig.suppressible && suppressINIWrites) else { return }
        _cpuClutRenderConfig.writer(_cpuClutRenderConfig.section, _cpuClutRenderConfig.key, cpuClutRender)
        _cpuClutRenderConfig.onSet?(cpuClutRender)
    }}
    let _cpuSpriteRenderBwConfig = Setting<Int>(
        section: "EmuCore/GS", key: "UserHacks_CPUSpriteRenderBW", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 0...10))) })
    var cpuSpriteRenderBw: Int = 0 { didSet {
        guard !(_cpuSpriteRenderBwConfig.suppressible && suppressINIWrites) else { return }
        _cpuSpriteRenderBwConfig.writer(_cpuSpriteRenderBwConfig.section, _cpuSpriteRenderBwConfig.key, cpuSpriteRenderBw)
        _cpuSpriteRenderBwConfig.onSet?(cpuSpriteRenderBw)
    }}
    let _cpuSpriteRenderLevelConfig = Setting<Int>(
        section: "EmuCore/GS", key: "UserHacks_CPUSpriteRenderLevel", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 0...2))) })
    var cpuSpriteRenderLevel: Int = 0 { didSet {
        guard !(_cpuSpriteRenderLevelConfig.suppressible && suppressINIWrites) else { return }
        _cpuSpriteRenderLevelConfig.writer(_cpuSpriteRenderLevelConfig.section, _cpuSpriteRenderLevelConfig.key, cpuSpriteRenderLevel)
        _cpuSpriteRenderLevelConfig.onSet?(cpuSpriteRenderLevel)
    }}
    let _gpuTargetClutConfig = Setting<Int>(
        section: "EmuCore/GS", key: "UserHacks_GPUTargetCLUTMode", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 0...2))) })
    var gpuTargetClut: Int = 0 { didSet {
        guard !(_gpuTargetClutConfig.suppressible && suppressINIWrites) else { return }
        _gpuTargetClutConfig.writer(_gpuTargetClutConfig.section, _gpuTargetClutConfig.key, gpuTargetClut)
        _gpuTargetClutConfig.onSet?(gpuTargetClut)
    }}
    let _bilinearUpscaleHackConfig = Setting<Int>(
        section: "EmuCore/GS", key: "UserHacks_BilinearHack", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 0...2))) })
    var bilinearUpscaleHack: Int = 0 { didSet {
        guard !(_bilinearUpscaleHackConfig.suppressible && suppressINIWrites) else { return }
        _bilinearUpscaleHackConfig.writer(_bilinearUpscaleHackConfig.section, _bilinearUpscaleHackConfig.key, bilinearUpscaleHack)
        _bilinearUpscaleHackConfig.onSet?(bilinearUpscaleHack)
    }}
    let _maxAnisotropyConfig = Setting<Int>(
        section: "EmuCore/GS", key: "MaxAnisotropy", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 0...16))) },
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var maxAnisotropy: Int = 0 { didSet {
        guard !(_maxAnisotropyConfig.suppressible && suppressINIWrites) else { return }
        _maxAnisotropyConfig.writer(_maxAnisotropyConfig.section, _maxAnisotropyConfig.key, maxAnisotropy)
        _maxAnisotropyConfig.onSet?(maxAnisotropy)
    }}
    let _hardwareDownloadModeConfig = Setting<Int>(
        section: "EmuCore/GS", key: "HWDownloadMode", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 0...4))) })
    var hardwareDownloadMode: Int = 0 { didSet {
        guard !(_hardwareDownloadModeConfig.suppressible && suppressINIWrites) else { return }
        _hardwareDownloadModeConfig.writer(_hardwareDownloadModeConfig.section, _hardwareDownloadModeConfig.key, hardwareDownloadMode)
        _hardwareDownloadModeConfig.onSet?(hardwareDownloadMode)
    }}
    let _tvShaderConfig = Setting<Int>(
        section: "EmuCore/GS", key: "TVShader", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 0...7))) },
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var tvShader: Int = 0 { didSet {
        guard !(_tvShaderConfig.suppressible && suppressINIWrites) else { return }
        _tvShaderConfig.writer(_tvShaderConfig.section, _tvShaderConfig.key, tvShader)
        _tvShaderConfig.onSet?(tvShader)
    }}
    // MetalFX Spatial upscaler (0 = Off / bilinear, 1 = MetalFX Spatial).
    // Hidden in the UI when isMetalFXAvailable is false; default is Off.
    let _upscalerConfig = Setting<Int>(
        section: "EmuCore/GS", key: "Upscaler", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) },
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var upscaler: Int = 0 { didSet {
        guard !(_upscalerConfig.suppressible && suppressINIWrites) else { return }
        _upscalerConfig.writer(_upscalerConfig.section, _upscalerConfig.key, upscaler)
        _upscalerConfig.onSet?(upscaler)
    }}

    /// Homogeneous bool GS hacks — see SettingsStore+Graphics.swift for the option list.
    var gsBoolHacks: [String: Bool] = [:]

    func gsBoolHackEnabled(_ key: String) -> Bool {
        gsBoolHacks[key] ?? false
    }

    func setGSBoolHack(_ key: String, _ value: Bool) {
        gsBoolHacks[key] = value
        guard !suppressINIWrites else { return }
        ARMSX2Bridge.setINIBool("EmuCore/GS", key: key, value: value)
    }

    private static func loadGSBoolHacks() -> [String: Bool] {
        var values: [String: Bool] = [:]
        for option in gsBoolHackOptions {
            values[option.key] = ARMSX2Bridge.getINIBool("EmuCore/GS", key: option.key, defaultValue: false)
        }
        return values
    }

    // ── Screen / PCRTC (EmuCore/GS) ── display-output options, applied live.
    let _pcrtcOffsetsConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "pcrtc_offsets", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool,
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var pcrtcOffsets: Bool = false { didSet {
        guard !(_pcrtcOffsetsConfig.suppressible && suppressINIWrites) else { return }
        _pcrtcOffsetsConfig.writer(_pcrtcOffsetsConfig.section, _pcrtcOffsetsConfig.key, pcrtcOffsets)
        _pcrtcOffsetsConfig.onSet?(pcrtcOffsets)
    }}
    let _pcrtcOverscanConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "pcrtc_overscan", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool,
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var pcrtcOverscan: Bool = false { didSet {
        guard !(_pcrtcOverscanConfig.suppressible && suppressINIWrites) else { return }
        _pcrtcOverscanConfig.writer(_pcrtcOverscanConfig.section, _pcrtcOverscanConfig.key, pcrtcOverscan)
        _pcrtcOverscanConfig.onSet?(pcrtcOverscan)
    }}
    let _pcrtcAntiBlurConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "pcrtc_antiblur", default: true,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool,
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var pcrtcAntiBlur: Bool = true { didSet {
        guard !(_pcrtcAntiBlurConfig.suppressible && suppressINIWrites) else { return }
        _pcrtcAntiBlurConfig.writer(_pcrtcAntiBlurConfig.section, _pcrtcAntiBlurConfig.key, pcrtcAntiBlur)
        _pcrtcAntiBlurConfig.onSet?(pcrtcAntiBlur)
    }}
    let _disableInterlaceOffsetConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "disable_interlace_offset", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool,
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var disableInterlaceOffset: Bool = false { didSet {
        guard !(_disableInterlaceOffsetConfig.suppressible && suppressINIWrites) else { return }
        _disableInterlaceOffsetConfig.writer(_disableInterlaceOffsetConfig.section, _disableInterlaceOffsetConfig.key, disableInterlaceOffset)
        _disableInterlaceOffsetConfig.onSet?(disableInterlaceOffset)
    }}
    let _skipDuplicateFramesConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "SkipDuplicateFrames", default: true,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool,
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var skipDuplicateFrames: Bool = true { didSet {
        guard !(_skipDuplicateFramesConfig.suppressible && suppressINIWrites) else { return }
        _skipDuplicateFramesConfig.writer(_skipDuplicateFramesConfig.section, _skipDuplicateFramesConfig.key, skipDuplicateFrames)
        _skipDuplicateFramesConfig.onSet?(skipDuplicateFrames)
    }}
    let _syncToHostRefreshConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "SyncToHostRefreshRate", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var syncToHostRefresh: Bool = false { didSet {
        guard !(_syncToHostRefreshConfig.suppressible && suppressINIWrites) else { return }
        _syncToHostRefreshConfig.writer(_syncToHostRefreshConfig.section, _syncToHostRefreshConfig.key, syncToHostRefresh)
        _syncToHostRefreshConfig.onSet?(syncToHostRefresh)
        markFramePacingCustom()
    }}
    let _integerScalingConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "IntegerScaling", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool,
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var integerScaling: Bool = false { didSet {
        guard !(_integerScalingConfig.suppressible && suppressINIWrites) else { return }
        _integerScalingConfig.writer(_integerScalingConfig.section, _integerScalingConfig.key, integerScaling)
        _integerScalingConfig.onSet?(integerScaling)
    }}

    // ── Shade Boost (EmuCore/GS) ── post-process color adjustment, applied live.
    let _shadeBoostConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "ShadeBoost", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool,
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var shadeBoost: Bool = false { didSet {
        guard !(_shadeBoostConfig.suppressible && suppressINIWrites) else { return }
        _shadeBoostConfig.writer(_shadeBoostConfig.section, _shadeBoostConfig.key, shadeBoost)
        _shadeBoostConfig.onSet?(shadeBoost)
    }}
    let _shadeBoostBrightnessConfig = Setting<Int>(
        section: "EmuCore/GS", key: "ShadeBoost_Brightness", default: 50,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 1...100))) },
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var shadeBoostBrightness: Int = 50 { didSet {
        guard !(_shadeBoostBrightnessConfig.suppressible && suppressINIWrites) else { return }
        _shadeBoostBrightnessConfig.writer(_shadeBoostBrightnessConfig.section, _shadeBoostBrightnessConfig.key, shadeBoostBrightness)
        _shadeBoostBrightnessConfig.onSet?(shadeBoostBrightness)
    }}
    let _shadeBoostContrastConfig = Setting<Int>(
        section: "EmuCore/GS", key: "ShadeBoost_Contrast", default: 50,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 1...100))) },
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var shadeBoostContrast: Int = 50 { didSet {
        guard !(_shadeBoostContrastConfig.suppressible && suppressINIWrites) else { return }
        _shadeBoostContrastConfig.writer(_shadeBoostContrastConfig.section, _shadeBoostContrastConfig.key, shadeBoostContrast)
        _shadeBoostContrastConfig.onSet?(shadeBoostContrast)
    }}
    let _shadeBoostSaturationConfig = Setting<Int>(
        section: "EmuCore/GS", key: "ShadeBoost_Saturation", default: 50,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 1...100))) },
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var shadeBoostSaturation: Int = 50 { didSet {
        guard !(_shadeBoostSaturationConfig.suppressible && suppressINIWrites) else { return }
        _shadeBoostSaturationConfig.writer(_shadeBoostSaturationConfig.section, _shadeBoostSaturationConfig.key, shadeBoostSaturation)
        _shadeBoostSaturationConfig.onSet?(shadeBoostSaturation)
    }}
    let _shadeBoostGammaConfig = Setting<Int>(
        section: "EmuCore/GS", key: "ShadeBoost_Gamma", default: 50,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(SettingsStore.clamped(v, to: 1...100))) },
        onSet: { _ in SettingsStore.shared.requestGraphicsApplyGuarded() })
    var shadeBoostGamma: Int = 50 { didSet {
        guard !(_shadeBoostGammaConfig.suppressible && suppressINIWrites) else { return }
        _shadeBoostGammaConfig.writer(_shadeBoostGammaConfig.section, _shadeBoostGammaConfig.key, shadeBoostGamma)
        _shadeBoostGammaConfig.onSet?(shadeBoostGamma)
    }}

    // ── OSD Overlay ──
    var osdPreset: OsdPreset {
        didSet {
            // Only an explicit user change should cascade the preset into the
            // individual OSD flags. During a bulk reload (suppressINIWrites), skip
            // so applyOsdPreset() can't overwrite the user OSD settings.
            guard !suppressINIWrites else { return }
            ARMSX2Bridge.setINIInt("ARMSX2iOS/UI", key: "OsdPreset", value: Int32(osdPreset.rawValue))
            if osdPreset == .off {
                if oldValue != .off {
                    lastActiveOsdPreset = oldValue
                }
            } else {
                lastActiveOsdPreset = osdPreset
            }
            if osdPreset == .custom {
                if !isAutoMarkingCustom {
                    restoreCustomOsd()
                }
            } else {
                applyOsdPreset(osdPreset)
            }
        }
    }
    // Frame Pacing — consolidated EmuCore/GS + SPU2/Output + Framerate surface.
    var framePacingPreset: FramePacingPreset = .optimal {
        didSet {
            // Only an explicit user change cascades the preset into the
            // individual pacing keys; skip during a bulk reload so restored
            // values are not overwritten.
            guard !suppressINIWrites else { return }
            ARMSX2Bridge.setINIInt("ARMSX2iOS/FramePacing", key: "Preset", value: Int32(framePacingPreset.rawValue))
            if framePacingPreset == .custom {
                if !isAutoMarkingFramePacingCustom {
                    restoreCustomFramePacing()
                }
            } else {
                applyFramePacingPreset(framePacingPreset)
            }
        }
    }
    // Adaptive Resolution — opt-in frame-time-driven dynamic internal
    // resolution, off by default. The didSet writes the INI key and starts or
    // stops the controller so its lifecycle tracks the user's toggle.
    let _adaptiveResolutionEnabledConfig = Setting<Bool>(
        section: "ARMSX2iOS/FramePacing", key: "DynamicResolution", default: false,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIBool(s, key: k, value: v) })
    var adaptiveResolutionEnabled: Bool = false { didSet {
        guard !(_adaptiveResolutionEnabledConfig.suppressible && suppressINIWrites) else { return }
        _adaptiveResolutionEnabledConfig.writer(_adaptiveResolutionEnabledConfig.section, _adaptiveResolutionEnabledConfig.key, adaptiveResolutionEnabled)
        _adaptiveResolutionEnabledConfig.onSet?(adaptiveResolutionEnabled)
        FrameTimeDynamicResolutionController.shared.setEnabled(adaptiveResolutionEnabled)
    }}
    let _lastActiveOsdPresetConfig = Setting<OsdPreset>(
        section: "ARMSX2iOS/UI", key: "LastActiveOsdPreset", default: .simple,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v.rawValue)) })
    var lastActiveOsdPreset: OsdPreset = .simple { didSet {
        guard !(_lastActiveOsdPresetConfig.suppressible && suppressINIWrites) else { return }
        _lastActiveOsdPresetConfig.writer(_lastActiveOsdPresetConfig.section, _lastActiveOsdPresetConfig.key, lastActiveOsdPreset)
        _lastActiveOsdPresetConfig.onSet?(lastActiveOsdPreset)
    }}
    let _osdPerformancePositionConfig = Setting<Int>(
        section: "EmuCore/GS", key: "OsdPerformancePos", default: 3,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) })
    var osdPerformancePosition: Int = 3 { didSet {
        guard !(_osdPerformancePositionConfig.suppressible && suppressINIWrites) else { return }
        _osdPerformancePositionConfig.writer(_osdPerformancePositionConfig.section, _osdPerformancePositionConfig.key, osdPerformancePosition)
        _osdPerformancePositionConfig.onSet?(osdPerformancePosition)
    }}
    /// Suppresses transient on-screen messages (shader compilation, save state,
    /// settings-applied). Critical SwiftUI alerts are unaffected. Backed by the
    /// core's OsdMessagesPos (1 = TopLeft default, 0 = None).
    let _osdShowMessagesConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "OsdMessagesPos", default: true,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: v ? 1 : 0) })
    var osdShowMessages: Bool = true { didSet {
        guard !(_osdShowMessagesConfig.suppressible && suppressINIWrites) else { return }
        _osdShowMessagesConfig.writer(_osdShowMessagesConfig.section, _osdShowMessagesConfig.key, osdShowMessages)
        _osdShowMessagesConfig.onSet?(osdShowMessages)
    }}
    let _osdShowFPSConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "OsdShowFPS", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var osdShowFPS: Bool = false { didSet {
        guard !(_osdShowFPSConfig.suppressible && suppressINIWrites) else { return }
        _osdShowFPSConfig.writer(_osdShowFPSConfig.section, _osdShowFPSConfig.key, osdShowFPS)
        _osdShowFPSConfig.onSet?(osdShowFPS)
        markOsdCustom()
    }}
    let _osdShowVPSConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "OsdShowVPS", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var osdShowVPS: Bool = false { didSet {
        guard !(_osdShowVPSConfig.suppressible && suppressINIWrites) else { return }
        _osdShowVPSConfig.writer(_osdShowVPSConfig.section, _osdShowVPSConfig.key, osdShowVPS)
        _osdShowVPSConfig.onSet?(osdShowVPS)
        markOsdCustom()
    }}
    let _osdShowSpeedConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "OsdShowSpeed", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var osdShowSpeed: Bool = false { didSet {
        guard !(_osdShowSpeedConfig.suppressible && suppressINIWrites) else { return }
        _osdShowSpeedConfig.writer(_osdShowSpeedConfig.section, _osdShowSpeedConfig.key, osdShowSpeed)
        _osdShowSpeedConfig.onSet?(osdShowSpeed)
        markOsdCustom()
    }}
    let _osdShowCPUConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "OsdShowCPU", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var osdShowCPU: Bool = false { didSet {
        guard !(_osdShowCPUConfig.suppressible && suppressINIWrites) else { return }
        _osdShowCPUConfig.writer(_osdShowCPUConfig.section, _osdShowCPUConfig.key, osdShowCPU)
        _osdShowCPUConfig.onSet?(osdShowCPU)
        markOsdCustom()
    }}
    let _osdShowGPUConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "OsdShowGPU", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var osdShowGPU: Bool = false { didSet {
        guard !(_osdShowGPUConfig.suppressible && suppressINIWrites) else { return }
        _osdShowGPUConfig.writer(_osdShowGPUConfig.section, _osdShowGPUConfig.key, osdShowGPU)
        _osdShowGPUConfig.onSet?(osdShowGPU)
        markOsdCustom()
    }}
    let _osdShowResolutionConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "OsdShowResolution", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var osdShowResolution: Bool = false { didSet {
        guard !(_osdShowResolutionConfig.suppressible && suppressINIWrites) else { return }
        _osdShowResolutionConfig.writer(_osdShowResolutionConfig.section, _osdShowResolutionConfig.key, osdShowResolution)
        _osdShowResolutionConfig.onSet?(osdShowResolution)
        markOsdCustom()
    }}
    let _osdShowGSStatsConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "OsdShowGSStats", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var osdShowGSStats: Bool = false { didSet {
        guard !(_osdShowGSStatsConfig.suppressible && suppressINIWrites) else { return }
        _osdShowGSStatsConfig.writer(_osdShowGSStatsConfig.section, _osdShowGSStatsConfig.key, osdShowGSStats)
        _osdShowGSStatsConfig.onSet?(osdShowGSStats)
        markOsdCustom()
    }}
    let _osdShowIndicatorsConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "OsdShowIndicators", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var osdShowIndicators: Bool = false { didSet {
        guard !(_osdShowIndicatorsConfig.suppressible && suppressINIWrites) else { return }
        _osdShowIndicatorsConfig.writer(_osdShowIndicatorsConfig.section, _osdShowIndicatorsConfig.key, osdShowIndicators)
        _osdShowIndicatorsConfig.onSet?(osdShowIndicators)
        markOsdCustom()
    }}
    let _osdShowSettingsConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "OsdShowSettings", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var osdShowSettings: Bool = false { didSet {
        guard !(_osdShowSettingsConfig.suppressible && suppressINIWrites) else { return }
        _osdShowSettingsConfig.writer(_osdShowSettingsConfig.section, _osdShowSettingsConfig.key, osdShowSettings)
        _osdShowSettingsConfig.onSet?(osdShowSettings)
        markOsdCustom()
    }}
    let _osdShowInputsConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "OsdShowInputs", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var osdShowInputs: Bool = false { didSet {
        guard !(_osdShowInputsConfig.suppressible && suppressINIWrites) else { return }
        _osdShowInputsConfig.writer(_osdShowInputsConfig.section, _osdShowInputsConfig.key, osdShowInputs)
        _osdShowInputsConfig.onSet?(osdShowInputs)
        markOsdCustom()
    }}
    let _osdShowFrameTimesConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "OsdShowFrameTimes", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var osdShowFrameTimes: Bool = false { didSet {
        guard !(_osdShowFrameTimesConfig.suppressible && suppressINIWrites) else { return }
        _osdShowFrameTimesConfig.writer(_osdShowFrameTimesConfig.section, _osdShowFrameTimesConfig.key, osdShowFrameTimes)
        _osdShowFrameTimesConfig.onSet?(osdShowFrameTimes)
        markOsdCustom()
    }}
    let _osdShowVersionConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "OsdShowVersion", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var osdShowVersion: Bool = false { didSet {
        guard !(_osdShowVersionConfig.suppressible && suppressINIWrites) else { return }
        _osdShowVersionConfig.writer(_osdShowVersionConfig.section, _osdShowVersionConfig.key, osdShowVersion)
        _osdShowVersionConfig.onSet?(osdShowVersion)
        markOsdCustom()
    }}
    let _osdShowHardwareInfoConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "OsdShowHardwareInfo", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var osdShowHardwareInfo: Bool = false { didSet {
        guard !(_osdShowHardwareInfoConfig.suppressible && suppressINIWrites) else { return }
        _osdShowHardwareInfoConfig.writer(_osdShowHardwareInfoConfig.section, _osdShowHardwareInfoConfig.key, osdShowHardwareInfo)
        _osdShowHardwareInfoConfig.onSet?(osdShowHardwareInfo)
        markOsdCustom()
    }}
    let _osdShowTextureReplacementsConfig = Setting<Bool>(
        section: "EmuCore/GS", key: "OsdShowTextureReplacements", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var osdShowTextureReplacements: Bool = false { didSet {
        guard !(_osdShowTextureReplacementsConfig.suppressible && suppressINIWrites) else { return }
        _osdShowTextureReplacementsConfig.writer(_osdShowTextureReplacementsConfig.section, _osdShowTextureReplacementsConfig.key, osdShowTextureReplacements)
        _osdShowTextureReplacementsConfig.onSet?(osdShowTextureReplacements)
    }}
    let _osdShowDeviceStatsConfig = Setting<Bool>(
        section: "ARMSX2iOS/UI", key: "OsdShowDeviceStats", default: false,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var osdShowDeviceStats: Bool = false { didSet {
        guard !(_osdShowDeviceStatsConfig.suppressible && suppressINIWrites) else { return }
        _osdShowDeviceStatsConfig.writer(_osdShowDeviceStatsConfig.section, _osdShowDeviceStatsConfig.key, osdShowDeviceStats)
        _osdShowDeviceStatsConfig.onSet?(osdShowDeviceStats)
        markOsdCustom()
    }}

    // ── Gamepad / UI ──
    let _padOpacityConfig = Setting<Float>(
        section: "ARMSX2iOS/UI", key: "PadOpacity", default: 0.6,
        suppressible: false,
        writer: ARMSX2Bridge.setINIFloat)
    var padOpacity: Float = 0.6 { didSet {
        guard !(_padOpacityConfig.suppressible && suppressINIWrites) else { return }
        _padOpacityConfig.writer(_padOpacityConfig.section, _padOpacityConfig.key, padOpacity)
        _padOpacityConfig.onSet?(padOpacity)
    }}
    let _hapticFeedbackConfig = Setting<Bool>(
        section: "ARMSX2iOS/UI", key: "HapticFeedback", default: true,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var hapticFeedback: Bool = true { didSet {
        guard !(_hapticFeedbackConfig.suppressible && suppressINIWrites) else { return }
        _hapticFeedbackConfig.writer(_hapticFeedbackConfig.section, _hapticFeedbackConfig.key, hapticFeedback)
        _hapticFeedbackConfig.onSet?(hapticFeedback)
    }}
    let _dpadDiagonalsEnabledConfig = Setting<Bool>(
        section: "ARMSX2iOS/UI", key: "DpadDiagonalsEnabled", default: true,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var dpadDiagonalsEnabled: Bool = true { didSet {
        guard !(_dpadDiagonalsEnabledConfig.suppressible && suppressINIWrites) else { return }
        _dpadDiagonalsEnabledConfig.writer(_dpadDiagonalsEnabledConfig.section, _dpadDiagonalsEnabledConfig.key, dpadDiagonalsEnabled)
        _dpadDiagonalsEnabledConfig.onSet?(dpadDiagonalsEnabled)
    }}
    let _faceComboZonesEnabledConfig = Setting<Bool>(
        section: "ARMSX2iOS/UI", key: "FaceComboZonesEnabled", default: true,
        suppressible: false,
        writer: ARMSX2Bridge.setINIBool)
    var faceComboZonesEnabled: Bool = true { didSet {
        guard !(_faceComboZonesEnabledConfig.suppressible && suppressINIWrites) else { return }
        _faceComboZonesEnabledConfig.writer(_faceComboZonesEnabledConfig.section, _faceComboZonesEnabledConfig.key, faceComboZonesEnabled)
        _faceComboZonesEnabledConfig.onSet?(faceComboZonesEnabled)
    }}
    let _virtualPadSkinConfig = Setting<VirtualPadSkin>(
        section: "ARMSX2iOS/UI", key: "VirtualPadSkin", default: .armsx2Refresh,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v.rawValue)) })
    var virtualPadSkin: VirtualPadSkin = .armsx2Refresh { didSet {
        guard !(_virtualPadSkinConfig.suppressible && suppressINIWrites) else { return }
        _virtualPadSkinConfig.writer(_virtualPadSkinConfig.section, _virtualPadSkinConfig.key, virtualPadSkin)
        _virtualPadSkinConfig.onSet?(virtualPadSkin)
    }}
    let _autoHideVirtualPadWhenControllerConnectedConfig = Setting<Bool>(
        section: "ARMSX2iOS/UI", key: "AutoHideVirtualPadWhenControllerConnected", default: true,
        writer: ARMSX2Bridge.setINIBool)
    var autoHideVirtualPadWhenControllerConnected: Bool = true { didSet {
        guard !(_autoHideVirtualPadWhenControllerConnectedConfig.suppressible && suppressINIWrites) else { return }
        _autoHideVirtualPadWhenControllerConnectedConfig.writer(_autoHideVirtualPadWhenControllerConnectedConfig.section, _autoHideVirtualPadWhenControllerConnectedConfig.key, autoHideVirtualPadWhenControllerConnected)
        _autoHideVirtualPadWhenControllerConnectedConfig.onSet?(autoHideVirtualPadWhenControllerConnected)
    }}
    let _autoFullscreenConfig = Setting<Bool>(
        section: "ARMSX2iOS/UI", key: "AutoFullscreen", default: true,
        writer: ARMSX2Bridge.setINIBool)
    var autoFullscreen: Bool = true { didSet {
        guard !(_autoFullscreenConfig.suppressible && suppressINIWrites) else { return }
        _autoFullscreenConfig.writer(_autoFullscreenConfig.section, _autoFullscreenConfig.key, autoFullscreen)
        _autoFullscreenConfig.onSet?(autoFullscreen)
    }}
    let _hideMenuButtonConfig = Setting<Bool>(
        section: "ARMSX2iOS/UI", key: "HideMenuButton", default: false,
        writer: ARMSX2Bridge.setINIBool)
    var hideMenuButton: Bool = false { didSet {
        guard !(_hideMenuButtonConfig.suppressible && suppressINIWrites) else { return }
        _hideMenuButtonConfig.writer(_hideMenuButtonConfig.section, _hideMenuButtonConfig.key, hideMenuButton)
        _hideMenuButtonConfig.onSet?(hideMenuButton)
    }}
    // clamps to 0.8...1.6
    var analogStickScale: Float {
        didSet {
            let clamped = Self.clampedAnalogStickScale(analogStickScale)
            guard abs(analogStickScale - clamped) <= 0.001 else {
                analogStickScale = clamped
                return
            }
            guard !suppressINIWrites else { return }
            ARMSX2Bridge.setINIFloat("ARMSX2iOS/UI", key: "AnalogStickScale", value: analogStickScale)
        }
    }
    private static let stickInversionSection = "ARMSX2iOS/UI"
    let _invertLeftStickXConfig = Setting<Bool>(
        section: "ARMSX2iOS/UI", key: "InvertLeftStickX", default: false,
        writer: ARMSX2Bridge.setINIBool)
    var invertLeftStickX: Bool = false { didSet {
        guard !(_invertLeftStickXConfig.suppressible && suppressINIWrites) else { return }
        _invertLeftStickXConfig.writer(_invertLeftStickXConfig.section, _invertLeftStickXConfig.key, invertLeftStickX)
    }}
    let _invertLeftStickYConfig = Setting<Bool>(
        section: "ARMSX2iOS/UI", key: "InvertLeftStickY", default: false,
        writer: ARMSX2Bridge.setINIBool)
    var invertLeftStickY: Bool = false { didSet {
        guard !(_invertLeftStickYConfig.suppressible && suppressINIWrites) else { return }
        _invertLeftStickYConfig.writer(_invertLeftStickYConfig.section, _invertLeftStickYConfig.key, invertLeftStickY)
    }}
    let _invertRightStickXConfig = Setting<Bool>(
        section: "ARMSX2iOS/UI", key: "InvertRightStickX", default: false,
        writer: ARMSX2Bridge.setINIBool)
    var invertRightStickX: Bool = false { didSet {
        guard !(_invertRightStickXConfig.suppressible && suppressINIWrites) else { return }
        _invertRightStickXConfig.writer(_invertRightStickXConfig.section, _invertRightStickXConfig.key, invertRightStickX)
    }}
    let _invertRightStickYConfig = Setting<Bool>(
        section: "ARMSX2iOS/UI", key: "InvertRightStickY", default: false,
        writer: ARMSX2Bridge.setINIBool)
    var invertRightStickY: Bool = false { didSet {
        guard !(_invertRightStickYConfig.suppressible && suppressINIWrites) else { return }
        _invertRightStickYConfig.writer(_invertRightStickYConfig.section, _invertRightStickYConfig.key, invertRightStickY)
    }}

    /// Effective axis inversion for a stick, resolving a per-game override (current game INI)
    /// before the global default. Read live at the stick input choke point.
    func stickInversion(for side: StickSide) -> (x: Bool, y: Bool) {
        func resolve(_ key: String, global: Bool) -> Bool {
            if ARMSX2Bridge.hasPerGameINIValueForCurrentGame(Self.stickInversionSection, key: key) {
                return ARMSX2Bridge.getPerGameINIBoolForCurrentGame(Self.stickInversionSection, key: key, defaultValue: global)
            }
            return global
        }
        switch side {
        case .left: return (resolve("InvertLeftStickX", global: invertLeftStickX), resolve("InvertLeftStickY", global: invertLeftStickY))
        case .right: return (resolve("InvertRightStickX", global: invertRightStickX), resolve("InvertRightStickY", global: invertRightStickY))
        }
    }
    let _appLanguageConfig = Setting<AppLanguage>(
        section: "ARMSX2iOS/UI", key: "AppLanguage", default: .system,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIString(s, key: k, value: v.rawValue) })
    var appLanguage: AppLanguage = .system { didSet {
        guard !(_appLanguageConfig.suppressible && suppressINIWrites) else { return }
        _appLanguageConfig.writer(_appLanguageConfig.section, _appLanguageConfig.key, appLanguage)
        _appLanguageConfig.onSet?(appLanguage)
    }}
    let _controllerMultitapModeConfig = Setting<Int>(
        section: "ARMSX2iOS/Gamepad", key: "MultitapMode", default: 0,
        suppressible: false,
        writer: { s, k, v in ARMSX2Bridge.setINIInt(s, key: k, value: Int32(v)) })
    var controllerMultitapMode: Int = 0 { didSet {
        guard !(_controllerMultitapModeConfig.suppressible && suppressINIWrites) else { return }
        _controllerMultitapModeConfig.writer(_controllerMultitapModeConfig.section, _controllerMultitapModeConfig.key, controllerMultitapMode)
        _controllerMultitapModeConfig.onSet?(controllerMultitapMode)
    }}
    let _autoOpenStikDebugConfig = Setting<Bool>(
        section: "ARMSX2iOS/JIT", key: "AutoOpenStikDebug", default: false,
        writer: ARMSX2Bridge.setINIBool)
    var autoOpenStikDebug: Bool = false { didSet {
        guard !(_autoOpenStikDebugConfig.suppressible && suppressINIWrites) else { return }
        _autoOpenStikDebugConfig.writer(_autoOpenStikDebugConfig.section, _autoOpenStikDebugConfig.key, autoOpenStikDebug)
        _autoOpenStikDebugConfig.onSet?(autoOpenStikDebug)
    }}
    let _jitScriptProtocolConfig = Setting<JITScriptProtocol>(
        section: "ARMSX2iOS/JIT", key: "ScriptProtocol", default: .legacy,
        writer: { s, k, v in ARMSX2Bridge.setINIString(s, key: k, value: v.rawValue) })
    var jitScriptProtocol: JITScriptProtocol = .legacy { didSet {
        guard !(_jitScriptProtocolConfig.suppressible && suppressINIWrites) else { return }
        _jitScriptProtocolConfig.writer(_jitScriptProtocolConfig.section, _jitScriptProtocolConfig.key, jitScriptProtocol)
        _jitScriptProtocolConfig.onSet?(jitScriptProtocol)
    }}

    // DEV9 / Network
    // writes HddEnable + HddFile (+ excludes HDD image from backup on enable)
    var dev9HddEnabled: Bool {
        didSet {
            guard !suppressINIWrites else { return }
            ARMSX2Bridge.setINIBool("DEV9/Hdd", key: "HddEnable", value: dev9HddEnabled)
            ARMSX2Bridge.setINIString("DEV9/Hdd", key: "HddFile", value: dev9HddFile)
            if dev9HddEnabled {
                // A large HDD image should never ride along in iCloud/iTunes
                // backups, so mark it excluded as soon as the feature is on.
                excludeHddImageFromBackup()
            }
        }
    }
    let _dev9HddFileConfig = Setting<String>(
        section: "DEV9/Hdd", key: "HddFile", default: "DEV9hdd.raw",
        writer: ARMSX2Bridge.setINIString)
    var dev9HddFile: String = "DEV9hdd.raw" { didSet {
        guard !(_dev9HddFileConfig.suppressible && suppressINIWrites) else { return }
        _dev9HddFileConfig.writer(_dev9HddFileConfig.section, _dev9HddFileConfig.key, dev9HddFile)
        _dev9HddFileConfig.onSet?(dev9HddFile)
    }}
    // writes EthEnable + EthApi + EthDevice
    var dev9EthernetEnabled: Bool {
        didSet {
            guard !suppressINIWrites else { return }
            ARMSX2Bridge.setINIBool("DEV9/Eth", key: "EthEnable", value: dev9EthernetEnabled)
            if dev9EthernetEnabled {
                ARMSX2Bridge.setINIString("DEV9/Eth", key: "EthApi", value: "Sockets")
                ARMSX2Bridge.setINIString("DEV9/Eth", key: "EthDevice", value: dev9EthDevice.isEmpty ? "Auto" : dev9EthDevice)
            }
        }
    }
    // writes EthApi + EthDevice
    var dev9EthDevice: String {
        didSet {
            guard !suppressINIWrites else { return }
            ARMSX2Bridge.setINIString("DEV9/Eth", key: "EthApi", value: "Sockets")
            ARMSX2Bridge.setINIString("DEV9/Eth", key: "EthDevice", value: dev9EthDevice.isEmpty ? "Auto" : dev9EthDevice)
        }
    }
    let _dev9InterceptDHCPConfig = Setting<Bool>(
        section: "DEV9/Eth", key: "InterceptDHCP", default: false,
        writer: ARMSX2Bridge.setINIBool)
    var dev9InterceptDHCP: Bool = false { didSet {
        guard !(_dev9InterceptDHCPConfig.suppressible && suppressINIWrites) else { return }
        _dev9InterceptDHCPConfig.writer(_dev9InterceptDHCPConfig.section, _dev9InterceptDHCPConfig.key, dev9InterceptDHCP)
        _dev9InterceptDHCPConfig.onSet?(dev9InterceptDHCP)
    }}
    let _dev9EthLogDHCPConfig = Setting<Bool>(
        section: "DEV9/Eth", key: "EthLogDHCP", default: false,
        writer: ARMSX2Bridge.setINIBool)
    var dev9EthLogDHCP: Bool = false { didSet {
        guard !(_dev9EthLogDHCPConfig.suppressible && suppressINIWrites) else { return }
        _dev9EthLogDHCPConfig.writer(_dev9EthLogDHCPConfig.section, _dev9EthLogDHCPConfig.key, dev9EthLogDHCP)
        _dev9EthLogDHCPConfig.onSet?(dev9EthLogDHCP)
    }}
    let _dev9EthLogDNSConfig = Setting<Bool>(
        section: "DEV9/Eth", key: "EthLogDNS", default: false,
        writer: ARMSX2Bridge.setINIBool)
    var dev9EthLogDNS: Bool = false { didSet {
        guard !(_dev9EthLogDNSConfig.suppressible && suppressINIWrites) else { return }
        _dev9EthLogDNSConfig.writer(_dev9EthLogDNSConfig.section, _dev9EthLogDNSConfig.key, dev9EthLogDNS)
        _dev9EthLogDNSConfig.onSet?(dev9EthLogDNS)
    }}
    let _dev9DNS1ModeConfig = Setting<String>(
        section: "DEV9/Eth", key: "ModeDNS1", default: "Auto",
        writer: ARMSX2Bridge.setINIString)
    var dev9DNS1Mode: String = "Auto" { didSet {
        guard !(_dev9DNS1ModeConfig.suppressible && suppressINIWrites) else { return }
        _dev9DNS1ModeConfig.writer(_dev9DNS1ModeConfig.section, _dev9DNS1ModeConfig.key, dev9DNS1Mode)
        _dev9DNS1ModeConfig.onSet?(dev9DNS1Mode)
    }}
    let _dev9DNS1Config = Setting<String>(
        section: "DEV9/Eth", key: "DNS1", default: "0.0.0.0",
        writer: ARMSX2Bridge.setINIString)
    var dev9DNS1: String = "0.0.0.0" { didSet {
        guard !(_dev9DNS1Config.suppressible && suppressINIWrites) else { return }
        _dev9DNS1Config.writer(_dev9DNS1Config.section, _dev9DNS1Config.key, dev9DNS1)
        _dev9DNS1Config.onSet?(dev9DNS1)
    }}
    let _dev9DNS2ModeConfig = Setting<String>(
        section: "DEV9/Eth", key: "ModeDNS2", default: "Auto",
        writer: ARMSX2Bridge.setINIString)
    var dev9DNS2Mode: String = "Auto" { didSet {
        guard !(_dev9DNS2ModeConfig.suppressible && suppressINIWrites) else { return }
        _dev9DNS2ModeConfig.writer(_dev9DNS2ModeConfig.section, _dev9DNS2ModeConfig.key, dev9DNS2Mode)
        _dev9DNS2ModeConfig.onSet?(dev9DNS2Mode)
    }}
    let _dev9DNS2Config = Setting<String>(
        section: "DEV9/Eth", key: "DNS2", default: "0.0.0.0",
        writer: ARMSX2Bridge.setINIString)
    var dev9DNS2: String = "0.0.0.0" { didSet {
        guard !(_dev9DNS2Config.suppressible && suppressINIWrites) else { return }
        _dev9DNS2Config.writer(_dev9DNS2Config.section, _dev9DNS2Config.key, dev9DNS2)
        _dev9DNS2Config.onSet?(dev9DNS2)
    }}

    // ── Library Background ──
    var dynamicBackgroundsEnabled: Bool = true {
        didSet {
            UserDefaults.standard.set(
                dynamicBackgroundsEnabled,
                forKey: "ARMSX2iOSDynamicBackgroundsEnabled"
            )
        }
    }
    var dynamicAppearancePreferences: DynamicAppearancePreferences = .standard {
        didSet { dynamicAppearancePreferences.save() }
    }
    var backgroundPrimaryAsset: BackgroundAsset? {
        didSet {
            if let asset = backgroundPrimaryAsset {
                UserDefaults.standard.set(try? JSONEncoder().encode(asset), forKey: "ARMSX2iOSBackgroundPrimaryAsset")
            } else {
                UserDefaults.standard.removeObject(forKey: "ARMSX2iOSBackgroundPrimaryAsset")
            }
        }
    }
    var backgroundLandscapeAsset: BackgroundAsset? {
        didSet {
            if let asset = backgroundLandscapeAsset {
                UserDefaults.standard.set(try? JSONEncoder().encode(asset), forKey: "ARMSX2iOSBackgroundLandscapeAsset")
            } else {
                UserDefaults.standard.removeObject(forKey: "ARMSX2iOSBackgroundLandscapeAsset")
            }
        }
    }
    var backgroundFitMode: BackgroundFitMode {
        didSet { UserDefaults.standard.set(backgroundFitMode.rawValue, forKey: "ARMSX2iOSBackgroundFitMode") }
    }
    var backgroundLandscapeFitMode: BackgroundFitMode = .fill {
        didSet { UserDefaults.standard.set(backgroundLandscapeFitMode.rawValue, forKey: "ARMSX2iOSBackgroundLandscapeFitMode") }
    }
    var backgroundVideoMuted: Bool {
        didSet { UserDefaults.standard.set(backgroundVideoMuted, forKey: "ARMSX2iOSBackgroundVideoMuted") }
    }
    var backgroundDim: Double {
        didSet {
            let clamped = Self.clampedBackgroundDim(backgroundDim)
            guard backgroundDim == clamped else { backgroundDim = clamped; return }
            UserDefaults.standard.set(backgroundDim, forKey: "ARMSX2iOSBackgroundDim")
        }
    }
    var backgroundEnabledInBIOS: Bool = true {
        didSet {
            UserDefaults.standard.set(backgroundEnabledInBIOS, forKey: "ARMSX2iOSBackgroundEnabledInBIOS")
        }
    }
    var backgroundEnabledInHelp: Bool = false {
        didSet {
            UserDefaults.standard.set(backgroundEnabledInHelp, forKey: "ARMSX2iOSBackgroundEnabledInHelp")
        }
    }
    var backgroundEnabledInSettings: Bool = false {
        didSet {
            UserDefaults.standard.set(backgroundEnabledInSettings, forKey: "ARMSX2iOSBackgroundEnabledInSettings")
        }
    }

    var hasCustomBackground: Bool {
        dynamicBackgroundsEnabled
            || backgroundPrimaryAsset != nil
            || backgroundLandscapeAsset != nil
    }

    // aspectRatioName / aspectRatioValue — see SettingsStore+Graphics.swift.
    // loadedFastBoot / loadedJITScriptProtocol — see SettingsStore+Speedhacks.swift.

    // ── Init from INI ──
    private init() {
        // The wrapped properties now have inline default values, so assignments
        // here fire their didSet. Suppress INI writes during initialization so
        // reading from INI does not also write back. Matches reload()'s pattern.
        suppressINIWrites = true
        defer { suppressINIWrites = false }

        // Frame Pacing default migration. Runs before any stored property is
        // read so the values below are authoritative. Writes the INI directly
        // — can't touch SettingsStore.shared mid-init (swift_once deadlock).
        Self.migrateFramePacingOptimalDefaultV1()

        // CPU
        eeCoreType = Int(ARMSX2Bridge.getINIInt("EmuCore/CPU", key: "CoreType", defaultValue: 2))
        iopRecompiler = ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "EnableIOP", defaultValue: true)
        vu0Recompiler = ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "EnableVU0", defaultValue: true)
        vu1Recompiler = ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "EnableVU1", defaultValue: true)
        fastBoot = Self.loadedFastBoot()
        fastmem = ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "EnableFastmem", defaultValue: true)
        // CPU rounding & clamping
        eeFpuRoundMode = Self.clampedRoundMode(Int(ARMSX2Bridge.getINIInt("EmuCore/CPU", key: "FPU.Roundmode", defaultValue: 3)))
        vu0RoundMode = Self.clampedRoundMode(Int(ARMSX2Bridge.getINIInt("EmuCore/CPU", key: "VU0.Roundmode", defaultValue: 3)))
        vu1RoundMode = Self.clampedRoundMode(Int(ARMSX2Bridge.getINIInt("EmuCore/CPU", key: "VU1.Roundmode", defaultValue: 3)))
        eeClampMode = Self.eeClampModeFromBools(
            ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "fpuOverflow", defaultValue: true),
            ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "fpuExtraOverflow", defaultValue: false),
            ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "fpuFullMode", defaultValue: false))
        vuClampMode = Self.vuClampModeFromBools(
            ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "vu0Overflow", defaultValue: true),
            ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "vu0ExtraOverflow", defaultValue: false),
            ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "vu0SignOverflow", defaultValue: false))
        let loadedNTSCFramerate = ARMSX2Bridge.getINIFloat("EmuCore/GS", key: "FramerateNTSC", defaultValue: 59.94)
        ntscFramerate = loadedNTSCFramerate
        palFramerate = ARMSX2Bridge.getINIFloat("EmuCore/GS", key: "FrameratePAL", defaultValue: 50.0)
        let nominalScalar = ARMSX2Bridge.getINIFloat("Framerate", key: "NominalScalar", defaultValue: 1.0)
        frameLimiterEnabled = Self.frameLimiterEnabled(fromNominalScalar: nominalScalar)
        targetFPS = Self.targetFPS(fromNominalScalar: nominalScalar, baseFramerate: loadedNTSCFramerate)
        Self.sanitizeNominalScalarIfNeeded(nominalScalar)
        fastForwardScalar = Self.clampedSpeedScalar(ARMSX2Bridge.getINIFloat("Framerate", key: "TurboScalar", defaultValue: Self.defaultFastForwardScalar))
        emulatorVolumePercent = Self.clampedEmulatorVolumePercent(Int(ARMSX2Bridge.emulatorVolumePercent()))
        audioTimeStretch = ARMSX2Bridge.getINIString("SPU2/Output", key: "SyncMode", defaultValue: "TimeStretch") != "Disabled"
        audioBufferMs = Self.clamped(Int(ARMSX2Bridge.getINIInt("SPU2/Output", key: "BufferMS", defaultValue: 50)), to: 10...200)
        audioOutputLatencyMs = Self.clamped(Int(ARMSX2Bridge.getINIInt("SPU2/Output", key: "OutputLatencyMS", defaultValue: 20)), to: 5...200)
        audioFastForwardVolume = Self.clamped(Int(ARMSX2Bridge.getINIInt("SPU2/Output", key: "FastForwardVolume", defaultValue: 100)), to: 0...200)
        audioSwapChannels = ARMSX2Bridge.getINIBool("SPU2/Output", key: "SwapChannels", defaultValue: false)
        // Boot
        fastCDVD = ARMSX2Bridge.getINIBool("EmuCore/Speedhacks", key: "fastCDVD", defaultValue: false)
        // Advanced Speedhacks
        eeCycleRate = Int(ARMSX2Bridge.getINIInt("EmuCore/Speedhacks", key: "EECycleRate", defaultValue: 0))
        vu1Instant = ARMSX2Bridge.getINIBool("EmuCore/Speedhacks", key: "vu1Instant", defaultValue: true)
        mtvu = ARMSX2Bridge.getINIBool("EmuCore/Speedhacks", key: "vuThread", defaultValue: true)
        waitLoop = ARMSX2Bridge.getINIBool("EmuCore/Speedhacks", key: "WaitLoop", defaultValue: true)
        intcStat = ARMSX2Bridge.getINIBool("EmuCore/Speedhacks", key: "IntcStat", defaultValue: true)
        eeCycleSkip = Self.clampedCycleSkip(Int(ARMSX2Bridge.getINIInt("EmuCore/Speedhacks", key: "EECycleSkip", defaultValue: 0)))
        vuFlagHack = ARMSX2Bridge.getINIBool("EmuCore/Speedhacks", key: "vuFlagHack", defaultValue: true)
        enableCheats = ARMSX2Bridge.getINIBool("EmuCore", key: "EnableCheats", defaultValue: false)
        enablePatches = ARMSX2Bridge.getINIBool("EmuCore", key: "EnablePatches", defaultValue: true)
        enableGameFixes = ARMSX2Bridge.getINIBool("EmuCore", key: "EnableGameFixes", defaultValue: true)
        enableGameDBHardwareFixes = !ARMSX2Bridge.getINIBool("EmuCore/GS", key: "UserHacks", defaultValue: false)
        enableWidescreenPatches = ARMSX2Bridge.getINIBool("EmuCore", key: "EnableWideScreenPatches", defaultValue: false)
        enableNoInterlacingPatches = ARMSX2Bridge.getINIBool("EmuCore", key: "EnableNoInterlacingPatches", defaultValue: false)
        hostFilesystem = ARMSX2Bridge.getINIBool("EmuCore", key: "HostFs", defaultValue: false)
        gameFixes = Self.loadGameFixes()
        // Graphics
#if targetEnvironment(macCatalyst)
        renderer = 17
        ARMSX2Bridge.setINIInt("EmuCore/GS", key: "Renderer", value: Int32(17))
#else
        let initialRenderer = Self.supportedIOSRenderer(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "Renderer", defaultValue: 17)))
        renderer = initialRenderer
        ARMSX2Bridge.setINIInt("EmuCore/GS", key: "Renderer", value: Int32(initialRenderer))
#endif
        upscaleMultiplier = ARMSX2Bridge.getINIFloat("EmuCore/GS", key: "upscale_multiplier", defaultValue: 1.0)
        vsyncQueueSize = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "VsyncQueueSize", defaultValue: 8))
        textureFiltering = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "filter", defaultValue: 2))
        backThreadMode = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "GSBackThreadMode", defaultValue: 0))
        hardwareMipmapping = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "hw_mipmap", defaultValue: true)
        fxaa = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "fxaa", defaultValue: false)
        casMode = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "CASMode", defaultValue: 0))
        casSharpness = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "CASSharpness", defaultValue: 50))
        interlaceMode = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "deinterlace_mode", defaultValue: 7))
        aspectRatio = Self.aspectRatioValue(from: ARMSX2Bridge.getINIString("EmuCore/GS", key: "AspectRatio", defaultValue: "Auto 4:3/3:2"))
        blendingAccuracy = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "accurate_blending_unit", defaultValue: 1))
        dithering = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "dithering_ps2", defaultValue: 2))
        trilinearFiltering = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "TriFilter", defaultValue: -1))
        halfPixelOffset = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_HalfPixelOffset", defaultValue: 0))
        roundSprite = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_round_sprite_offset", defaultValue: 0))
        alignSprite = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "UserHacks_align_sprite_X", defaultValue: false)
        mergeSprite = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "UserHacks_merge_pp_sprite", defaultValue: false)
        wildArmsOffset = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "UserHacks_ForceEvenSpritePosition", defaultValue: false)
        textureOffsetX = Self.clampedTextureOffset(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_TCOffsetX", defaultValue: 0)))
        textureOffsetY = Self.clampedTextureOffset(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_TCOffsetY", defaultValue: 0)))
        let loadedSkipDrawStart = Self.clampedSkipDraw(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_SkipDraw_Start", defaultValue: 0)))
        skipDrawStart = loadedSkipDrawStart
        skipDrawEnd = Self.normalizedSkipDrawEnd(
            start: loadedSkipDrawStart,
            end: Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_SkipDraw_End", defaultValue: 0))
        )
        loadTextureReplacements = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "LoadTextureReplacements", defaultValue: false)
        loadTextureReplacementsAsync = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "LoadTextureReplacementsAsync", defaultValue: true)
        precacheTextureReplacements = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "PrecacheTextureReplacements", defaultValue: false)
        texturePreloading = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "texture_preloading", defaultValue: 2))
        dumpReplaceableTextures = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "DumpReplaceableTextures", defaultValue: false)
        dumpReplaceableMipmaps = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "DumpReplaceableMipmaps", defaultValue: false)
        dumpTexturesWithFMVActive = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "DumpTexturesWithFMVActive", defaultValue: false)
        dumpDirectTextures = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "DumpDirectTextures", defaultValue: true)
        dumpPaletteTextures = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "DumpPaletteTextures", defaultValue: true)
        // GS Hardware Fixes
        hwAccurateAlphaTest = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "HWAccurateAlphaTest", defaultValue: false)
        textureInsideRt = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_TextureInsideRt", defaultValue: 0)), to: 0...2)
        limit24BitDepth = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_Limit24BitDepth", defaultValue: 0)), to: 0...2)
        nativeScaling = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_native_scaling", defaultValue: 0)), to: 0...4)
        cpuClutRender = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_CPUCLUTRender", defaultValue: 0)), to: 0...2)
        cpuSpriteRenderBw = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_CPUSpriteRenderBW", defaultValue: 0)), to: 0...10)
        cpuSpriteRenderLevel = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_CPUSpriteRenderLevel", defaultValue: 0)), to: 0...2)
        gpuTargetClut = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_GPUTargetCLUTMode", defaultValue: 0)), to: 0...2)
        bilinearUpscaleHack = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_BilinearHack", defaultValue: 0)), to: 0...2)
        maxAnisotropy = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "MaxAnisotropy", defaultValue: 0)), to: 0...16)
        hardwareDownloadMode = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "HWDownloadMode", defaultValue: 0)), to: 0...4)
        tvShader = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "TVShader", defaultValue: 0)), to: 0...7)
        upscaler = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "Upscaler", defaultValue: 0))
        gsBoolHacks = Self.loadGSBoolHacks()
        // Screen / PCRTC
        pcrtcOffsets = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "pcrtc_offsets", defaultValue: false)
        pcrtcOverscan = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "pcrtc_overscan", defaultValue: false)
        pcrtcAntiBlur = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "pcrtc_antiblur", defaultValue: true)
        disableInterlaceOffset = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "disable_interlace_offset", defaultValue: false)
        skipDuplicateFrames = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "SkipDuplicateFrames", defaultValue: true)
        syncToHostRefresh = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "SyncToHostRefreshRate", defaultValue: false)
        integerScaling = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "IntegerScaling", defaultValue: false)
        // Shade Boost
        shadeBoost = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "ShadeBoost", defaultValue: false)
        shadeBoostBrightness = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "ShadeBoost_Brightness", defaultValue: 50)), to: 1...100)
        shadeBoostContrast = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "ShadeBoost_Contrast", defaultValue: 50)), to: 1...100)
        shadeBoostSaturation = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "ShadeBoost_Saturation", defaultValue: 50)), to: 1...100)
        shadeBoostGamma = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "ShadeBoost_Gamma", defaultValue: 50)), to: 1...100)
        // OSD
        let loadedOsdPreset = OsdPreset(rawValue: Int(ARMSX2Bridge.getINIInt("ARMSX2iOS/UI", key: "OsdPreset", defaultValue: 0))) ?? .off
        osdPreset = loadedOsdPreset
        let loadedLastActiveOsdPresetRaw = ARMSX2Bridge.getINIInt("ARMSX2iOS/UI", key: "LastActiveOsdPreset", defaultValue: -1)
        if loadedLastActiveOsdPresetRaw >= 0 {
            lastActiveOsdPreset = OsdPreset(rawValue: Int(loadedLastActiveOsdPresetRaw)) ?? .simple
        } else {
            lastActiveOsdPreset = loadedOsdPreset != .off ? loadedOsdPreset : .simple
        }
        osdPerformancePosition = Self.normalizedOsdPerformancePosition(
            Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "OsdPerformancePos", defaultValue: Int32(Self.defaultOsdPerformancePosition)))
        )
        osdShowMessages = ARMSX2Bridge.getINIInt("EmuCore/GS", key: "OsdMessagesPos", defaultValue: 1) != 0
        osdShowFPS = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowFPS", defaultValue: false)
        osdShowVPS = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowVPS", defaultValue: false)
        osdShowSpeed = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowSpeed", defaultValue: false)
        osdShowCPU = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowCPU", defaultValue: false)
        osdShowGPU = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowGPU", defaultValue: false)
        osdShowResolution = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowResolution", defaultValue: false)
        osdShowGSStats = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowGSStats", defaultValue: false)
        osdShowIndicators = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowIndicators", defaultValue: false)
        osdShowSettings = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowSettings", defaultValue: false)
        osdShowInputs = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowInputs", defaultValue: false)
        osdShowFrameTimes = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowFrameTimes", defaultValue: false)
        osdShowVersion = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowVersion", defaultValue: false)
        osdShowHardwareInfo = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowHardwareInfo", defaultValue: false)
        osdShowTextureReplacements = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowTextureReplacements", defaultValue: false)
        osdShowDeviceStats = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "OsdShowDeviceStats", defaultValue: loadedOsdPreset != .off)
        // UI
        padOpacity = ARMSX2Bridge.getINIFloat("ARMSX2iOS/UI", key: "PadOpacity", defaultValue: 0.6)
        hapticFeedback = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "HapticFeedback", defaultValue: true)
        dpadDiagonalsEnabled = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "DpadDiagonalsEnabled", defaultValue: true)
        faceComboZonesEnabled = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "FaceComboZonesEnabled", defaultValue: true)
        virtualPadSkin = VirtualPadSkin(rawValue: Int(ARMSX2Bridge.getINIInt("ARMSX2iOS/UI", key: "VirtualPadSkin", defaultValue: 0))) ?? .armsx2Refresh
        autoHideVirtualPadWhenControllerConnected = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "AutoHideVirtualPadWhenControllerConnected", defaultValue: true)
        autoFullscreen = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "AutoFullscreen", defaultValue: true)
        hideMenuButton = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "HideMenuButton", defaultValue: false)
        analogStickScale = Self.clampedAnalogStickScale(ARMSX2Bridge.getINIFloat("ARMSX2iOS/UI", key: "AnalogStickScale", defaultValue: 1.0))
        invertLeftStickX = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "InvertLeftStickX", defaultValue: false)
        invertLeftStickY = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "InvertLeftStickY", defaultValue: false)
        invertRightStickX = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "InvertRightStickX", defaultValue: false)
        invertRightStickY = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "InvertRightStickY", defaultValue: false)
        appLanguage = AppLanguage(rawValue: ARMSX2Bridge.getINIString("ARMSX2iOS/UI", key: "AppLanguage", defaultValue: AppLanguage.system.rawValue)) ?? .system
        controllerMultitapMode = Int(ARMSX2Bridge.getINIInt("ARMSX2iOS/Gamepad", key: "MultitapMode", defaultValue: 0))
        autoOpenStikDebug = ARMSX2Bridge.getINIBool("ARMSX2iOS/JIT", key: "AutoOpenStikDebug", defaultValue: false)
        jitScriptProtocol = Self.loadedJITScriptProtocol()
        // The migration above already wrote the post-migration INI values;
        // these reads pick them up.
        framePacingPreset = FramePacingPreset(rawValue: Int(ARMSX2Bridge.getINIInt("ARMSX2iOS/FramePacing", key: "Preset", defaultValue: Int32(FramePacingPreset.optimal.rawValue)))) ?? .optimal
        // Adaptive Resolution: read back so a persisted ON state at boot starts
        // the controller. The didSet is suppressINIWrites-guarded so this
        // reassignment does not write back; setEnabled is called explicitly
        // below to sync the controller to the persisted value.
        let _initialAdaptiveResolution = ARMSX2Bridge.getINIBool("ARMSX2iOS/FramePacing", key: "DynamicResolution", defaultValue: false)
        adaptiveResolutionEnabled = _initialAdaptiveResolution
        dev9HddEnabled = ARMSX2Bridge.getINIBool("DEV9/Hdd", key: "HddEnable", defaultValue: false)
        dev9HddFile = ARMSX2Bridge.getINIString("DEV9/Hdd", key: "HddFile", defaultValue: "DEV9hdd.raw")
        dev9EthernetEnabled = ARMSX2Bridge.getINIBool("DEV9/Eth", key: "EthEnable", defaultValue: false)
        dev9EthDevice = ARMSX2Bridge.getINIString("DEV9/Eth", key: "EthDevice", defaultValue: "Auto")
        dev9InterceptDHCP = ARMSX2Bridge.getINIBool("DEV9/Eth", key: "InterceptDHCP", defaultValue: false)
        dev9EthLogDHCP = ARMSX2Bridge.getINIBool("DEV9/Eth", key: "EthLogDHCP", defaultValue: false)
        dev9EthLogDNS = ARMSX2Bridge.getINIBool("DEV9/Eth", key: "EthLogDNS", defaultValue: false)
        dev9DNS1Mode = ARMSX2Bridge.getINIString("DEV9/Eth", key: "ModeDNS1", defaultValue: "Auto")
        dev9DNS1 = ARMSX2Bridge.getINIString("DEV9/Eth", key: "DNS1", defaultValue: "0.0.0.0")
        dev9DNS2Mode = ARMSX2Bridge.getINIString("DEV9/Eth", key: "ModeDNS2", defaultValue: "Auto")
        dev9DNS2 = ARMSX2Bridge.getINIString("DEV9/Eth", key: "DNS2", defaultValue: "0.0.0.0")
        BackgroundStorage.migrateLegacyBackgroundsIfNeeded()
        dynamicBackgroundsEnabled = UserDefaults.standard.object(
            forKey: "ARMSX2iOSDynamicBackgroundsEnabled"
        ) as? Bool ?? true
        dynamicAppearancePreferences = DynamicAppearancePreferences.load() ?? .standard
        backgroundPrimaryAsset = Self.loadBackgroundAsset(forKey: "ARMSX2iOSBackgroundPrimaryAsset")
        backgroundLandscapeAsset = Self.loadBackgroundAsset(forKey: "ARMSX2iOSBackgroundLandscapeAsset")
        backgroundFitMode = BackgroundFitMode(rawValue: UserDefaults.standard.string(forKey: "ARMSX2iOSBackgroundFitMode") ?? "") ?? .fill
        backgroundLandscapeFitMode = BackgroundFitMode(rawValue: UserDefaults.standard.string(forKey: "ARMSX2iOSBackgroundLandscapeFitMode") ?? "") ?? .fill
        backgroundVideoMuted = UserDefaults.standard.object(forKey: "ARMSX2iOSBackgroundVideoMuted") as? Bool ?? true
        backgroundDim = Self.clampedBackgroundDim(UserDefaults.standard.object(forKey: "ARMSX2iOSBackgroundDim") as? Double ?? 0.0)
        backgroundEnabledInBIOS = UserDefaults.standard.object(forKey: "ARMSX2iOSBackgroundEnabledInBIOS") as? Bool ?? true
        backgroundEnabledInHelp = UserDefaults.standard.object(forKey: "ARMSX2iOSBackgroundEnabledInHelp") as? Bool ?? false
        backgroundEnabledInSettings = UserDefaults.standard.object(forKey: "ARMSX2iOSBackgroundEnabledInSettings") as? Bool ?? false
        normalizeDEV9Settings()
        VPadSkinLibraryStore.shared.adoptLegacySelection(virtualPadSkin)
        ARMSX2Bridge.setINIString("EmuCore/GS", key: "AspectRatio", value: Self.aspectRatioName(for: aspectRatio))
        // Do NOT re-apply the OSD preset here. The saved per-item OSD flags are the
        // source of truth and are pushed into the live GSConfig natively by
        // ARMSX2ApplyIOSOsdPresetFromConfig() at scene startup. Calling
        // applyOsdPreset(preset) at load rewrote every flag from the preset and
        // discarded the user settings.
        // Seed the Custom OSD snapshot once from the loaded flags so cycling to Custom
        // before any manual edit shows the current set rather than an empty overlay.
        if !ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "OsdCustomSeeded", defaultValue: false) {
            snapshotCustomOsd()
            ARMSX2Bridge.setINIBool("ARMSX2iOS/UI", key: "OsdCustomSeeded", value: true)
        }
        // Defer starting the adaptive controller to the next run loop: setEnabled
        // reads SettingsStore.shared.upscaleMultiplier, which would re-enter this
        // init's swift_once and deadlock dispatch_once.
        DispatchQueue.main.async { [self] in
            FrameTimeDynamicResolutionController.shared.setEnabled(self.adaptiveResolutionEnabled)
        }
    }

    /// Reload ALL settings from INI (call on VM start/stop)
    func reload() {
        suppressINIWrites = true
        defer { suppressINIWrites = false }

        eeCoreType = Int(ARMSX2Bridge.getINIInt("EmuCore/CPU", key: "CoreType", defaultValue: 2))
        iopRecompiler = ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "EnableIOP", defaultValue: true)
        vu0Recompiler = ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "EnableVU0", defaultValue: true)
        vu1Recompiler = ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "EnableVU1", defaultValue: true)
        fastBoot = Self.loadedFastBoot()
        fastmem = ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "EnableFastmem", defaultValue: true)
        eeFpuRoundMode = Self.clampedRoundMode(Int(ARMSX2Bridge.getINIInt("EmuCore/CPU", key: "FPU.Roundmode", defaultValue: 3)))
        vu0RoundMode = Self.clampedRoundMode(Int(ARMSX2Bridge.getINIInt("EmuCore/CPU", key: "VU0.Roundmode", defaultValue: 3)))
        vu1RoundMode = Self.clampedRoundMode(Int(ARMSX2Bridge.getINIInt("EmuCore/CPU", key: "VU1.Roundmode", defaultValue: 3)))
        eeClampMode = Self.eeClampModeFromBools(
            ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "fpuOverflow", defaultValue: true),
            ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "fpuExtraOverflow", defaultValue: false),
            ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "fpuFullMode", defaultValue: false))
        vuClampMode = Self.vuClampModeFromBools(
            ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "vu0Overflow", defaultValue: true),
            ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "vu0ExtraOverflow", defaultValue: false),
            ARMSX2Bridge.getINIBool("EmuCore/CPU/Recompiler", key: "vu0SignOverflow", defaultValue: false))
        ntscFramerate = ARMSX2Bridge.getINIFloat("EmuCore/GS", key: "FramerateNTSC", defaultValue: 59.94)
        palFramerate = ARMSX2Bridge.getINIFloat("EmuCore/GS", key: "FrameratePAL", defaultValue: 50.0)
        let nominalScalar = ARMSX2Bridge.getINIFloat("Framerate", key: "NominalScalar", defaultValue: 1.0)
        frameLimiterEnabled = Self.frameLimiterEnabled(fromNominalScalar: nominalScalar)
        targetFPS = Self.targetFPS(fromNominalScalar: nominalScalar, baseFramerate: ntscFramerate)
        Self.sanitizeNominalScalarIfNeeded(nominalScalar)
        fastForwardScalar = Self.clampedSpeedScalar(ARMSX2Bridge.getINIFloat("Framerate", key: "TurboScalar", defaultValue: Self.defaultFastForwardScalar))
        emulatorVolumePercent = Self.clampedEmulatorVolumePercent(Int(ARMSX2Bridge.emulatorVolumePercent()))
        audioTimeStretch = ARMSX2Bridge.getINIString("SPU2/Output", key: "SyncMode", defaultValue: "TimeStretch") != "Disabled"
        audioBufferMs = Self.clamped(Int(ARMSX2Bridge.getINIInt("SPU2/Output", key: "BufferMS", defaultValue: 50)), to: 10...200)
        audioOutputLatencyMs = Self.clamped(Int(ARMSX2Bridge.getINIInt("SPU2/Output", key: "OutputLatencyMS", defaultValue: 20)), to: 5...200)
        audioFastForwardVolume = Self.clamped(Int(ARMSX2Bridge.getINIInt("SPU2/Output", key: "FastForwardVolume", defaultValue: 100)), to: 0...200)
        audioSwapChannels = ARMSX2Bridge.getINIBool("SPU2/Output", key: "SwapChannels", defaultValue: false)
        fastCDVD = ARMSX2Bridge.getINIBool("EmuCore/Speedhacks", key: "fastCDVD", defaultValue: false)
        eeCycleRate = Int(ARMSX2Bridge.getINIInt("EmuCore/Speedhacks", key: "EECycleRate", defaultValue: 0))
        vu1Instant = ARMSX2Bridge.getINIBool("EmuCore/Speedhacks", key: "vu1Instant", defaultValue: true)
        mtvu = ARMSX2Bridge.getINIBool("EmuCore/Speedhacks", key: "vuThread", defaultValue: true)
        waitLoop = ARMSX2Bridge.getINIBool("EmuCore/Speedhacks", key: "WaitLoop", defaultValue: true)
        intcStat = ARMSX2Bridge.getINIBool("EmuCore/Speedhacks", key: "IntcStat", defaultValue: true)
        eeCycleSkip = Self.clampedCycleSkip(Int(ARMSX2Bridge.getINIInt("EmuCore/Speedhacks", key: "EECycleSkip", defaultValue: 0)))
        vuFlagHack = ARMSX2Bridge.getINIBool("EmuCore/Speedhacks", key: "vuFlagHack", defaultValue: true)
        enableCheats = ARMSX2Bridge.getINIBool("EmuCore", key: "EnableCheats", defaultValue: false)
        enablePatches = ARMSX2Bridge.getINIBool("EmuCore", key: "EnablePatches", defaultValue: true)
        enableGameFixes = ARMSX2Bridge.getINIBool("EmuCore", key: "EnableGameFixes", defaultValue: true)
        enableGameDBHardwareFixes = !ARMSX2Bridge.getINIBool("EmuCore/GS", key: "UserHacks", defaultValue: false)
        enableWidescreenPatches = ARMSX2Bridge.getINIBool("EmuCore", key: "EnableWideScreenPatches", defaultValue: false)
        enableNoInterlacingPatches = ARMSX2Bridge.getINIBool("EmuCore", key: "EnableNoInterlacingPatches", defaultValue: false)
        hostFilesystem = ARMSX2Bridge.getINIBool("EmuCore", key: "HostFs", defaultValue: false)
        gameFixes = Self.loadGameFixes()
#if targetEnvironment(macCatalyst)
        renderer = 17
        ARMSX2Bridge.setINIInt("EmuCore/GS", key: "Renderer", value: Int32(17))
#else
        renderer = Self.supportedIOSRenderer(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "Renderer", defaultValue: 17)))
        ARMSX2Bridge.setINIInt("EmuCore/GS", key: "Renderer", value: Int32(renderer))
#endif
        upscaleMultiplier = ARMSX2Bridge.getINIFloat("EmuCore/GS", key: "upscale_multiplier", defaultValue: 1.0)
        vsyncQueueSize = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "VsyncQueueSize", defaultValue: 8))
        textureFiltering = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "filter", defaultValue: 2))
        backThreadMode = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "GSBackThreadMode", defaultValue: 0))
        hardwareMipmapping = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "hw_mipmap", defaultValue: true)
        fxaa = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "fxaa", defaultValue: false)
        casMode = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "CASMode", defaultValue: 0))
        casSharpness = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "CASSharpness", defaultValue: 50))
        interlaceMode = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "deinterlace_mode", defaultValue: 7))
        aspectRatio = Self.aspectRatioValue(from: ARMSX2Bridge.getINIString("EmuCore/GS", key: "AspectRatio", defaultValue: "Auto 4:3/3:2"))
        blendingAccuracy = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "accurate_blending_unit", defaultValue: 1))
        dithering = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "dithering_ps2", defaultValue: 2))
        trilinearFiltering = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "TriFilter", defaultValue: -1))
        halfPixelOffset = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_HalfPixelOffset", defaultValue: 0))
        roundSprite = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_round_sprite_offset", defaultValue: 0))
        alignSprite = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "UserHacks_align_sprite_X", defaultValue: false)
        mergeSprite = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "UserHacks_merge_pp_sprite", defaultValue: false)
        wildArmsOffset = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "UserHacks_ForceEvenSpritePosition", defaultValue: false)
        textureOffsetX = Self.clampedTextureOffset(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_TCOffsetX", defaultValue: 0)))
        textureOffsetY = Self.clampedTextureOffset(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_TCOffsetY", defaultValue: 0)))
        let loadedSkipDrawStart = Self.clampedSkipDraw(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_SkipDraw_Start", defaultValue: 0)))
        skipDrawStart = loadedSkipDrawStart
        skipDrawEnd = Self.normalizedSkipDrawEnd(
            start: loadedSkipDrawStart,
            end: Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_SkipDraw_End", defaultValue: 0))
        )
        loadTextureReplacements = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "LoadTextureReplacements", defaultValue: false)
        loadTextureReplacementsAsync = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "LoadTextureReplacementsAsync", defaultValue: true)
        precacheTextureReplacements = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "PrecacheTextureReplacements", defaultValue: false)
        texturePreloading = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "texture_preloading", defaultValue: 2))
        dumpReplaceableTextures = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "DumpReplaceableTextures", defaultValue: false)
        dumpReplaceableMipmaps = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "DumpReplaceableMipmaps", defaultValue: false)
        dumpTexturesWithFMVActive = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "DumpTexturesWithFMVActive", defaultValue: false)
        dumpDirectTextures = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "DumpDirectTextures", defaultValue: true)
        dumpPaletteTextures = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "DumpPaletteTextures", defaultValue: true)
        // GS Hardware Fixes
        hwAccurateAlphaTest = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "HWAccurateAlphaTest", defaultValue: false)
        textureInsideRt = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_TextureInsideRt", defaultValue: 0)), to: 0...2)
        limit24BitDepth = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_Limit24BitDepth", defaultValue: 0)), to: 0...2)
        nativeScaling = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_native_scaling", defaultValue: 0)), to: 0...4)
        cpuClutRender = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_CPUCLUTRender", defaultValue: 0)), to: 0...2)
        cpuSpriteRenderBw = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_CPUSpriteRenderBW", defaultValue: 0)), to: 0...10)
        cpuSpriteRenderLevel = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_CPUSpriteRenderLevel", defaultValue: 0)), to: 0...2)
        gpuTargetClut = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_GPUTargetCLUTMode", defaultValue: 0)), to: 0...2)
        bilinearUpscaleHack = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "UserHacks_BilinearHack", defaultValue: 0)), to: 0...2)
        maxAnisotropy = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "MaxAnisotropy", defaultValue: 0)), to: 0...16)
        hardwareDownloadMode = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "HWDownloadMode", defaultValue: 0)), to: 0...4)
        tvShader = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "TVShader", defaultValue: 0)), to: 0...7)
        upscaler = Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "Upscaler", defaultValue: 0))
        gsBoolHacks = Self.loadGSBoolHacks()
        // Screen / PCRTC
        pcrtcOffsets = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "pcrtc_offsets", defaultValue: false)
        pcrtcOverscan = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "pcrtc_overscan", defaultValue: false)
        pcrtcAntiBlur = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "pcrtc_antiblur", defaultValue: true)
        disableInterlaceOffset = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "disable_interlace_offset", defaultValue: false)
        skipDuplicateFrames = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "SkipDuplicateFrames", defaultValue: true)
        syncToHostRefresh = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "SyncToHostRefreshRate", defaultValue: false)
        integerScaling = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "IntegerScaling", defaultValue: false)
        // Shade Boost
        shadeBoost = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "ShadeBoost", defaultValue: false)
        shadeBoostBrightness = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "ShadeBoost_Brightness", defaultValue: 50)), to: 1...100)
        shadeBoostContrast = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "ShadeBoost_Contrast", defaultValue: 50)), to: 1...100)
        shadeBoostSaturation = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "ShadeBoost_Saturation", defaultValue: 50)), to: 1...100)
        shadeBoostGamma = Self.clamped(Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "ShadeBoost_Gamma", defaultValue: 50)), to: 1...100)
        osdPreset = OsdPreset(rawValue: Int(ARMSX2Bridge.getINIInt("ARMSX2iOS/UI", key: "OsdPreset", defaultValue: 0))) ?? .off
        let loadedLastActiveOsdPresetRaw = ARMSX2Bridge.getINIInt("ARMSX2iOS/UI", key: "LastActiveOsdPreset", defaultValue: -1)
        if loadedLastActiveOsdPresetRaw >= 0 {
            lastActiveOsdPreset = OsdPreset(rawValue: Int(loadedLastActiveOsdPresetRaw)) ?? .simple
        } else {
            lastActiveOsdPreset = osdPreset != .off ? osdPreset : .simple
        }
        osdPerformancePosition = Self.normalizedOsdPerformancePosition(
            Int(ARMSX2Bridge.getINIInt("EmuCore/GS", key: "OsdPerformancePos", defaultValue: Int32(Self.defaultOsdPerformancePosition)))
        )
        osdShowMessages = ARMSX2Bridge.getINIInt("EmuCore/GS", key: "OsdMessagesPos", defaultValue: 1) != 0
        osdShowFPS = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowFPS", defaultValue: false)
        osdShowVPS = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowVPS", defaultValue: false)
        osdShowSpeed = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowSpeed", defaultValue: false)
        osdShowCPU = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowCPU", defaultValue: false)
        osdShowGPU = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowGPU", defaultValue: false)
        osdShowResolution = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowResolution", defaultValue: false)
        osdShowGSStats = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowGSStats", defaultValue: false)
        osdShowIndicators = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowIndicators", defaultValue: false)
        osdShowSettings = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowSettings", defaultValue: false)
        osdShowInputs = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowInputs", defaultValue: false)
        osdShowFrameTimes = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowFrameTimes", defaultValue: false)
        osdShowVersion = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowVersion", defaultValue: false)
        osdShowHardwareInfo = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowHardwareInfo", defaultValue: false)
        osdShowTextureReplacements = ARMSX2Bridge.getINIBool("EmuCore/GS", key: "OsdShowTextureReplacements", defaultValue: false)
        osdShowDeviceStats = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "OsdShowDeviceStats", defaultValue: osdPreset != .off)
        padOpacity = ARMSX2Bridge.getINIFloat("ARMSX2iOS/UI", key: "PadOpacity", defaultValue: 0.6)
        hapticFeedback = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "HapticFeedback", defaultValue: true)
        dpadDiagonalsEnabled = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "DpadDiagonalsEnabled", defaultValue: true)
        faceComboZonesEnabled = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "FaceComboZonesEnabled", defaultValue: true)
        virtualPadSkin = VirtualPadSkin(rawValue: Int(ARMSX2Bridge.getINIInt("ARMSX2iOS/UI", key: "VirtualPadSkin", defaultValue: 0))) ?? .armsx2Refresh
        autoHideVirtualPadWhenControllerConnected = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "AutoHideVirtualPadWhenControllerConnected", defaultValue: true)
        autoFullscreen = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "AutoFullscreen", defaultValue: true)
        hideMenuButton = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "HideMenuButton", defaultValue: false)
        analogStickScale = Self.clampedAnalogStickScale(ARMSX2Bridge.getINIFloat("ARMSX2iOS/UI", key: "AnalogStickScale", defaultValue: 1.0))
        invertLeftStickX = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "InvertLeftStickX", defaultValue: false)
        invertLeftStickY = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "InvertLeftStickY", defaultValue: false)
        invertRightStickX = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "InvertRightStickX", defaultValue: false)
        invertRightStickY = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: "InvertRightStickY", defaultValue: false)
        appLanguage = AppLanguage(rawValue: ARMSX2Bridge.getINIString("ARMSX2iOS/UI", key: "AppLanguage", defaultValue: AppLanguage.system.rawValue)) ?? .system
        controllerMultitapMode = Int(ARMSX2Bridge.getINIInt("ARMSX2iOS/Gamepad", key: "MultitapMode", defaultValue: 0))
        autoOpenStikDebug = ARMSX2Bridge.getINIBool("ARMSX2iOS/JIT", key: "AutoOpenStikDebug", defaultValue: false)
        jitScriptProtocol = Self.loadedJITScriptProtocol()
        dev9HddEnabled = ARMSX2Bridge.getINIBool("DEV9/Hdd", key: "HddEnable", defaultValue: false)
        dev9HddFile = ARMSX2Bridge.getINIString("DEV9/Hdd", key: "HddFile", defaultValue: "DEV9hdd.raw")
        if dev9HddEnabled { excludeHddImageFromBackup() }
        dev9EthernetEnabled = ARMSX2Bridge.getINIBool("DEV9/Eth", key: "EthEnable", defaultValue: false)
        dev9EthDevice = ARMSX2Bridge.getINIString("DEV9/Eth", key: "EthDevice", defaultValue: "Auto")
        dev9InterceptDHCP = ARMSX2Bridge.getINIBool("DEV9/Eth", key: "InterceptDHCP", defaultValue: false)
        dev9EthLogDHCP = ARMSX2Bridge.getINIBool("DEV9/Eth", key: "EthLogDHCP", defaultValue: false)
        dev9EthLogDNS = ARMSX2Bridge.getINIBool("DEV9/Eth", key: "EthLogDNS", defaultValue: false)
        dev9DNS1Mode = ARMSX2Bridge.getINIString("DEV9/Eth", key: "ModeDNS1", defaultValue: "Auto")
        dev9DNS1 = ARMSX2Bridge.getINIString("DEV9/Eth", key: "DNS1", defaultValue: "0.0.0.0")
        dev9DNS2Mode = ARMSX2Bridge.getINIString("DEV9/Eth", key: "ModeDNS2", defaultValue: "Auto")
        dev9DNS2 = ARMSX2Bridge.getINIString("DEV9/Eth", key: "DNS2", defaultValue: "0.0.0.0")
        dynamicBackgroundsEnabled = UserDefaults.standard.object(
            forKey: "ARMSX2iOSDynamicBackgroundsEnabled"
        ) as? Bool ?? true
        dynamicAppearancePreferences = DynamicAppearancePreferences.load() ?? .standard
        backgroundPrimaryAsset = Self.loadBackgroundAsset(forKey: "ARMSX2iOSBackgroundPrimaryAsset")
        backgroundLandscapeAsset = Self.loadBackgroundAsset(forKey: "ARMSX2iOSBackgroundLandscapeAsset")
        backgroundFitMode = BackgroundFitMode(rawValue: UserDefaults.standard.string(forKey: "ARMSX2iOSBackgroundFitMode") ?? "") ?? .fill
        backgroundLandscapeFitMode = BackgroundFitMode(rawValue: UserDefaults.standard.string(forKey: "ARMSX2iOSBackgroundLandscapeFitMode") ?? "") ?? .fill
        backgroundVideoMuted = UserDefaults.standard.object(forKey: "ARMSX2iOSBackgroundVideoMuted") as? Bool ?? true
        backgroundDim = Self.clampedBackgroundDim(UserDefaults.standard.object(forKey: "ARMSX2iOSBackgroundDim") as? Double ?? 0.0)
        backgroundEnabledInBIOS = UserDefaults.standard.object(forKey: "ARMSX2iOSBackgroundEnabledInBIOS") as? Bool ?? true
        backgroundEnabledInHelp = UserDefaults.standard.object(forKey: "ARMSX2iOSBackgroundEnabledInHelp") as? Bool ?? false
        backgroundEnabledInSettings = UserDefaults.standard.object(forKey: "ARMSX2iOSBackgroundEnabledInSettings") as? Bool ?? false
        normalizeDEV9Settings()
        VPadSkinLibraryStore.shared.adoptLegacySelection(virtualPadSkin)
    }

    static func frameLimiterEnabled(fromNominalScalar scalar: Float) -> Bool {
        scalar < 5.0
    }

    private static func sanitizedNominalScalar(_ scalar: Float) -> Float {
        guard scalar.isFinite else { return 1.0 }
        return min(max(scalar, 0.05), 10.0)
    }

    private static func clampedTargetFPS(_ fps: Float) -> Float {
        guard fps.isFinite else { return defaultTargetFPS }
        return min(max(fps.rounded(), minTargetFPS), maxTargetFPS)
    }

    private static func clampedSpeedScalar(_ scalar: Float) -> Float {
        guard scalar.isFinite else { return defaultFastForwardScalar }
        let stepped = (scalar * 4.0).rounded() / 4.0
        return min(max(stepped, minFastForwardScalar), maxFastForwardScalar)
    }

    // clampedEmulatorVolumePercent — see SettingsStore+Audio.swift.

    private static func clampedAnalogStickScale(_ scale: Float) -> Float {
        guard scale.isFinite else { return 1.0 }
        return min(max(scale, 0.8), 1.6)
    }

    private static func clampedBackgroundDim(_ value: Double) -> Double {
        guard value.isFinite else { return 0.0 }
        return min(max(value, 0.0), 1.0)
    }

    private static func loadBackgroundAsset(forKey key: String) -> BackgroundAsset? {
        guard let data = UserDefaults.standard.data(forKey: key) else { return nil }
        return try? JSONDecoder().decode(BackgroundAsset.self, from: data)
    }

    /// Removes background assets whose files no longer exist. Returns `true`
    /// when at least one stale asset was cleared so callers can surface a notice.
    @discardableResult
    func sanitizeBackgroundAssets() -> Bool {
        var removed = false
        if let primary = backgroundPrimaryAsset, !BackgroundStorage.exists(primary) {
            backgroundPrimaryAsset = nil
            removed = true
        }
        if let landscape = backgroundLandscapeAsset, !BackgroundStorage.exists(landscape) {
            backgroundLandscapeAsset = nil
            removed = true
        }
        return removed
    }

    private static func clampedTextureOffset(_ offset: Int) -> Int {
        min(max(offset, textureOffsetRange.lowerBound), textureOffsetRange.upperBound)
    }

    private static func clampedSkipDraw(_ value: Int) -> Int {
        min(max(value, skipDrawRange.lowerBound), skipDrawRange.upperBound)
    }

    static func normalizedSkipDrawEnd(start: Int, end: Int) -> Int {
        let clampedStart = clampedSkipDraw(start)
        let clampedEnd = clampedSkipDraw(end)
        return clampedStart > 0 && clampedEnd < clampedStart ? clampedStart : clampedEnd
    }

    // MARK: - CPU rounding/clamp writers

    /// Write the EE clamp level to the three FPU recompiler keys.
    static func applyEEClampMode(_ mode: Int) {
        ARMSX2Bridge.setINIBool("EmuCore/CPU/Recompiler", key: "fpuOverflow", value: mode >= 1)
        ARMSX2Bridge.setINIBool("EmuCore/CPU/Recompiler", key: "fpuExtraOverflow", value: mode >= 2)
        ARMSX2Bridge.setINIBool("EmuCore/CPU/Recompiler", key: "fpuFullMode", value: mode >= 3)
    }

    /// Write the VU clamp level to both VU0 and VU1 recompiler keys (six booleans total).
    static func applyVUClampMode(_ mode: Int) {
        for prefix in ["vu0", "vu1"] {
            ARMSX2Bridge.setINIBool("EmuCore/CPU/Recompiler", key: "\(prefix)Overflow", value: mode >= 1)
            ARMSX2Bridge.setINIBool("EmuCore/CPU/Recompiler", key: "\(prefix)ExtraOverflow", value: mode >= 2)
            ARMSX2Bridge.setINIBool("EmuCore/CPU/Recompiler", key: "\(prefix)SignOverflow", value: mode >= 3)
        }
    }

    static func targetFPS(fromNominalScalar scalar: Float, baseFramerate: Float) -> Float {
        guard frameLimiterEnabled(fromNominalScalar: scalar) else { return defaultTargetFPS }
        return clampedTargetFPS(sanitizedNominalScalar(scalar) * max(baseFramerate, 1.0))
    }

    private static func sanitizeNominalScalarIfNeeded(_ scalar: Float) {
        let sanitized = sanitizedNominalScalar(scalar)
        guard abs(scalar - sanitized) > 0.001 else { return }

        NSLog("[ARMSX2 iOS Settings] Clamping unsupported NominalScalar %.3f -> %.3f", scalar, sanitized)
        ARMSX2Bridge.setINIFloat("Framerate", key: "NominalScalar", value: sanitized)
    }

    // excludeHddImageFromBackup / normalizeDEV9Settings — see SettingsStore+DEV9.swift.

    private static func normalizedOsdPerformancePosition(_ value: Int) -> Int {
        switch value {
        case 0, 1, 3:
            return value
        case 2:
            return defaultOsdPerformancePosition
        default:
            return defaultOsdPerformancePosition
        }
    }

    private func applyFrameLimiterSettings() {
        guard !suppressINIWrites else { return }
        var scalar: Float = Self.nominalScalarForFrameLimiter(enabled: frameLimiterEnabled, targetFPS: targetFPS, baseFramerate: ntscFramerate)
        if ARMSX2Bridge.isRetroAchievementsHardcoreActive(), scalar < 1.0 {
            scalar = 1.0
        }
        NSLog("[ARMSX2 iOS Settings] Frame limiter %@ targetFPS=%.0f NominalScalar=%.3f",
              frameLimiterEnabled ? "ON" : "OFF", targetFPS, scalar)
        ARMSX2Bridge.setINIFloat("Framerate", key: "NominalScalar", value: scalar)
    }

    /// Encode the frame limiter as Framerate/NominalScalar: targetFPS/base when
    /// on, or 10.0 to disable. Shared by the global setter and per-game save.
    static func nominalScalarForFrameLimiter(enabled: Bool, targetFPS: Float, baseFramerate: Float) -> Float {
        enabled ? sanitizedNominalScalar(targetFPS / max(baseFramerate, 1.0)) : 10.0
    }

    func setRuntimeFastForwardEnabled(_ enabled: Bool) {
        fastForwardRuntimeEnabled = enabled
        // Fast forward is purely a limiter-mode switch (Nominal <-> Turbo). The
        // previous implementation also flipped frameLimiterEnabled, whose didSet
        // writes NominalScalar=10 to the INI — that made the OSD report T: 1000%
        // (the Nominal scalar) while the real turbo target was the FF scalar, and
        // churned the INI on every toggle. Turbo mode alone is sufficient: the
        // core computes the target from TurboScalar while in Turbo, and switching
        // back to Nominal restores the user's normal target (T: 100%).
        if enabled {
            NSLog("@@FF_UI@@ enabled=1 turbo=%.3f", fastForwardScalar)
            ARMSX2Bridge.setLimiterMode(1)
        } else {
            NSLog("@@FF_UI@@ enabled=0 targetFPS=%.0f", targetFPS)
            frameLimiterDisabledForFastForward = false
            ARMSX2Bridge.setLimiterMode(0)
        }
    }

    // supportedIOSRenderer — see SettingsStore+Graphics.swift.
    // localized / localizedLayoutDirection — see SettingsStore+UI.swift.

    /// Apply OSD preset — writes ALL OSD flags to INI + GSConfig
    private func applyOsdPreset(_ preset: OsdPreset) {
        guard preset != .custom else { return }
        ARMSX2Bridge.applyOsdPreset(Int32(preset.rawValue))
        if preset == .off {
            osdPerformancePosition = 0
        } else {
            revealOsdPerformancePositionIfHidden()
        }
        let isSimple = preset == .simple
        let isDetail = preset == .detail
        let isFull = preset == .full
        isProgrammaticOsdFlagChange = true
        defer { isProgrammaticOsdFlagChange = false }
        osdShowFPS = isSimple || isDetail || isFull
        osdShowVPS = isDetail || isFull
        osdShowSpeed = isSimple || isDetail || isFull
        osdShowCPU = isSimple || isDetail || isFull
        osdShowGPU = isDetail || isFull
        osdShowResolution = isDetail || isFull
        osdShowGSStats = isFull
        osdShowIndicators = isDetail || isFull
        osdShowSettings = isFull
        osdShowInputs = isFull
        osdShowFrameTimes = isFull
        osdShowVersion = isSimple || isDetail || isFull
        osdShowHardwareInfo = isFull
        osdShowDeviceStats = isSimple || isDetail || isFull
    }

    /// If the perf overlay is at the hidden position (None/0), restore it to the default
    /// so newly-enabled perf stats become visible. Shared by applyOsdPreset + restoreCustomOsd.
    private func revealOsdPerformancePositionIfHidden() {
        if osdPerformancePosition == 0 {
            osdPerformancePosition = Self.defaultOsdPerformancePosition
        }
    }

    private static let osdCustomFlagKeyPaths: [(ReferenceWritableKeyPath<SettingsStore, Bool>, String)] = [
        (\.osdShowFPS, "OsdCustomShowFPS"),
        (\.osdShowVPS, "OsdCustomShowVPS"),
        (\.osdShowSpeed, "OsdCustomShowSpeed"),
        (\.osdShowCPU, "OsdCustomShowCPU"),
        (\.osdShowGPU, "OsdCustomShowGPU"),
        (\.osdShowResolution, "OsdCustomShowResolution"),
        (\.osdShowGSStats, "OsdCustomShowGSStats"),
        (\.osdShowIndicators, "OsdCustomShowIndicators"),
        (\.osdShowSettings, "OsdCustomShowSettings"),
        (\.osdShowInputs, "OsdCustomShowInputs"),
        (\.osdShowFrameTimes, "OsdCustomShowFrameTimes"),
        (\.osdShowVersion, "OsdCustomShowVersion"),
        (\.osdShowHardwareInfo, "OsdCustomShowHardwareInfo"),
        (\.osdShowDeviceStats, "OsdCustomShowDeviceStats"),
    ]

    private func snapshotCustomOsd() {
        for (keyPath, key) in Self.osdCustomFlagKeyPaths {
            ARMSX2Bridge.setINIBool("ARMSX2iOS/UI", key: key, value: self[keyPath: keyPath])
        }
    }

    private func restoreCustomOsd() {
        revealOsdPerformancePositionIfHidden()
        isProgrammaticOsdFlagChange = true
        for (keyPath, key) in Self.osdCustomFlagKeyPaths {
            self[keyPath: keyPath] = ARMSX2Bridge.getINIBool("ARMSX2iOS/UI", key: key, defaultValue: self[keyPath: keyPath])
        }
        isProgrammaticOsdFlagChange = false
    }

    private func markOsdCustom() {
        guard !suppressINIWrites, !isProgrammaticOsdFlagChange else { return }
        isAutoMarkingCustom = true
        if osdPreset != .custom {
            osdPreset = .custom
        }
        isAutoMarkingCustom = false
        snapshotCustomOsd()
    }

    /// Apply a preset via the individual clamped setters. Never writes
    /// Framerate/NominalScalar directly — frameLimiterEnabled + targetFPS go
    /// through applyFrameLimiterSettings. Non-Framerate keys first so the
    /// limiter flag is set before targetFPS fires applyFrameLimiterSettings.
    func applyFramePacingPreset(_ preset: FramePacingPreset) {
        guard preset != .custom else { return }
        isProgrammaticFramePacingFlagChange = true
        defer { isProgrammaticFramePacingFlagChange = false }
        switch preset {
        case .optimal:
            vsyncQueueSize = 4
            audioOutputLatencyMs = 15
            audioBufferMs = 50
            syncToHostRefresh = false
            frameLimiterEnabled = true
            targetFPS = 60
        case .smooth:
            vsyncQueueSize = 8
            audioOutputLatencyMs = 20
            audioBufferMs = 75
            syncToHostRefresh = false
            frameLimiterEnabled = true
            targetFPS = 60
        case .lowLatency:
            vsyncQueueSize = 2
            audioOutputLatencyMs = 10
            audioBufferMs = 30
            syncToHostRefresh = false
            frameLimiterEnabled = true
            targetFPS = 60
        case .batterySaver:
            vsyncQueueSize = 8
            audioOutputLatencyMs = 30
            audioBufferMs = 100
            syncToHostRefresh = false
            frameLimiterEnabled = true
            targetFPS = 45
        case .custom:
            break
        }
    }

    /// Hook for restoring individual pacing values when cycling back to
    /// .custom. Currently a no-op; kept so the preset didSet stays symmetric
    /// with the OSD preset handling.
    private func restoreCustomFramePacing() {
    }

    /// Mark the preset .custom when the user edits any individual pacing
    /// control directly.
    private func markFramePacingCustom() {
        guard !suppressINIWrites, !isProgrammaticFramePacingFlagChange else { return }
        isAutoMarkingFramePacingCustom = true
        if framePacingPreset != .custom {
            framePacingPreset = .custom
        }
        isAutoMarkingFramePacingCustom = false
    }

    /// Reset emulator settings to ARMSX2 iOS defaults
    func resetEmulatorDefaults() {
        eeCoreType = 2          // ARM64 JIT
        // Core uses EnableEE (not CoreType) to select interpreter vs recompiler.
        // Restore EnableEE=true so the core actually uses the recompiler again,
        // undoing any prior applyFullInterpreterPreset() that forced the interpreter.
        ARMSX2Bridge.setINIBool("EmuCore/CPU/Recompiler", key: "EnableEE", value: true)
        iopRecompiler = true
        vu0Recompiler = true
        vu1Recompiler = true
        fastBoot = false
        fastmem = true
        eeFpuRoundMode = 3      // Chop (Zero)
        vu0RoundMode = 3
        vu1RoundMode = 3
        eeClampMode = 1         // Normal
        vuClampMode = 1
        targetFPS = Self.defaultTargetFPS
        frameLimiterEnabled = true
        fastForwardRuntimeEnabled = false
        frameLimiterDisabledForFastForward = false
        fastForwardScalar = Self.defaultFastForwardScalar
        emulatorVolumePercent = Self.defaultEmulatorVolumePercent
        audioTimeStretch = true
        audioBufferMs = 50
        audioOutputLatencyMs = 20
        audioFastForwardVolume = 100
        audioSwapChannels = false
        ntscFramerate = 59.94
        palFramerate = 50.0
        fastCDVD = false
        eeCycleRate = 0
        vu1Instant = true
        mtvu = true
        waitLoop = true
        intcStat = true
        eeCycleSkip = 0
        vuFlagHack = true
        enableCheats = false
        enablePatches = true
        enableGameFixes = true
        enableGameDBHardwareFixes = true
        enableWidescreenPatches = false
        enableNoInterlacingPatches = false
        hostFilesystem = false
        for option in Self.gameFixOptions {
            gameFixes[option.key] = false
            ARMSX2Bridge.setINIBool("EmuCore/Gamefixes", key: option.key, value: false)
        }
        jitScriptProtocol = JITScriptProtocol.defaultValue
    }

    /// Keep EE/IOP/VU0 fast while isolating suspected VU1 JIT regressions.
    func applyVU1CompatibilityPreset() {
        eeCoreType = 2
        // Core uses EnableEE (not CoreType) to select interpreter vs recompiler.
        // Restore EnableEE=true so the core actually uses the EE recompiler again,
        // undoing any prior applyFullInterpreterPreset() that forced the interpreter.
        ARMSX2Bridge.setINIBool("EmuCore/CPU/Recompiler", key: "EnableEE", value: true)
        iopRecompiler = true
        vu0Recompiler = true
        vu1Recompiler = false
        vu1Instant = false
        mtvu = false
        fastmem = false
    }

    /// Slow diagnostic preset for crash isolation when dynarec state is suspect.
    func applyFullInterpreterPreset() {
        eeCoreType = 1
        // Core uses EnableEE (not CoreType) to select interpreter vs recompiler.
        // Must write EnableEE=false to actually force the EE interpreter.
        ARMSX2Bridge.setINIBool("EmuCore/CPU/Recompiler", key: "EnableEE", value: false)
        iopRecompiler = false
        vu0Recompiler = false
        vu1Recompiler = false
        vu1Instant = false
        mtvu = false
        fastmem = false
    }

    /// Reset graphics settings to ARMSX2 iOS defaults
    func resetGraphicsDefaults() {
        renderer = 17           // Metal
        upscaleMultiplier = 1.0 // Native PS2
        vsyncQueueSize = 8
        textureFiltering = 2    // Bilinear (PS2)
        backThreadMode = 0            // Disabled
        hardwareMipmapping = true
        fxaa = false
        casMode = 0             // Disabled
        casSharpness = 50
        interlaceMode = 7       // Adaptive
        aspectRatio = 1         // Auto 4:3/3:2
        blendingAccuracy = 1    // Basic
        dithering = 2           // Scaled
        trilinearFiltering = -1 // Automatic
        halfPixelOffset = 0
        roundSprite = 0
        alignSprite = false
        mergeSprite = false
        wildArmsOffset = false
        textureOffsetX = 0
        textureOffsetY = 0
        skipDrawStart = 0
        skipDrawEnd = 0
        // GS hardware fixes
        hwAccurateAlphaTest = false
        textureInsideRt = 0
        limit24BitDepth = 0
        nativeScaling = 0
        cpuClutRender = 0
        cpuSpriteRenderBw = 0
        cpuSpriteRenderLevel = 0
        gpuTargetClut = 0
        bilinearUpscaleHack = 0
        maxAnisotropy = 0
        hardwareDownloadMode = 0
        tvShader = 0
        upscaler = 0
        for option in Self.gsBoolHackOptions {
            gsBoolHacks[option.key] = false
            ARMSX2Bridge.setINIBool("EmuCore/GS", key: option.key, value: false)
        }
        // Screen / PCRTC and Shade Boost
        pcrtcOffsets = false
        pcrtcOverscan = false
        pcrtcAntiBlur = true
        disableInterlaceOffset = false
        skipDuplicateFrames = true
        syncToHostRefresh = false
        integerScaling = false
        shadeBoost = false
        shadeBoostBrightness = 50
        shadeBoostContrast = 50
        shadeBoostSaturation = 50
        shadeBoostGamma = 50
        // Texture pack and dump toggles are intentionally preserved.
    }
}
