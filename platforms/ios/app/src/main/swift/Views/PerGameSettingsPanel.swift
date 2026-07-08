// PerGameSettingsPanel.swift - Per-game overrides panel
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import PhotosUI
import UniformTypeIdentifiers
import UIKit

struct PerGameSettingsPanel: View {
    @Environment(\.dismiss) private var dismiss
    @State private var settings = SettingsStore.shared
    @State private var layoutPresets = PadLayoutPresetStore.shared
    @State private var skinLibrary = VPadSkinLibraryStore.shared

    private enum PerGameSettingsCategory: CaseIterable, Identifiable {
        case general, graphics, audio, cpu, pad, fixes, cheats

        var id: Self { self }

        var titleKey: String {
            switch self {
            case .general: return "General"
            case .graphics: return "Graphics"
            case .audio: return "Audio"
            case .cpu: return "CPU & Speedhacks"
            case .pad: return "Virtual Pad"
            case .fixes: return "Fixes & Compatibility"
            case .cheats: return "Cheats & Patches"
            }
        }

        var systemImage: String {
            switch self {
            case .general: return "slider.horizontal.3"
            case .graphics: return "paintbrush"
            case .audio: return "speaker.wave.2"
            case .cpu: return "cpu"
            case .pad: return "gamecontroller"
            case .fixes: return "wrench.and.screwdriver"
            case .cheats: return "rectangle.stack.badge.plus"
            }
        }
    }

    private static let useGlobalSentinel = -1
    private static let trilinearUseGlobalSentinel = Int(Int32.min)
    private static let eeCycleRateUseGlobalSentinel = Int(Int32.min)
    private static let fastBootUseGlobalSentinel = -1
    private static let fastBootOff = 0
    private static let fastBootOn = 1

    let game: ISOEntry
    let onDone: (() -> Void)?
    let savesToRunningGame: Bool

    @State private var enabled: Bool
    @State private var upscaleMultiplier: Float
    @State private var aspectRatio: String
    @State private var textureFiltering: Int
    @State private var hardwareMipmapping: Bool
    @State private var blendingAccuracy: Int
    @State private var interlaceMode: Int
    @State private var trilinearFiltering: Int
    @State private var halfPixelOffset: Int
    @State private var roundSprite: Int
    @State private var alignSpriteOverride: Bool
    @State private var alignSprite: Bool
    @State private var mergeSpriteOverride: Bool
    @State private var mergeSprite: Bool
    @State private var wildArmsOffsetOverride: Bool
    @State private var wildArmsOffset: Bool
    @State private var textureOffsetXOverride: Bool
    @State private var textureOffsetX: Int
    @State private var textureOffsetYOverride: Bool
    @State private var textureOffsetY: Int
    @State private var skipDrawStartOverride: Bool
    @State private var skipDrawStart: Int
    @State private var skipDrawEndOverride: Bool
    @State private var skipDrawEnd: Int
    @State private var globalVolumePercent: Int
    @State private var volumeOverride: Bool
    @State private var volumePercent: Int
    @State private var padLayoutIdentity: PadLayoutGameIdentity?
    @State private var showPadLayoutEditor = false
    @State private var eeCoreType: Int
    @State private var mtvu: Bool
    @State private var globalEECycleRate: Int
    @State private var eeCycleRate: Int
    @State private var globalEECycleSkip: Int
    @State private var eeCycleSkip: Int
    @State private var globalFastBoot: Bool
    @State private var fastBoot: Int
    @State private var hasGameSettingsIdentity: Bool
    @State private var enableCheats: Bool
    @State private var enablePatches: Bool
    @State private var enableGameFixes: Bool
    @State private var enableGameDBHardwareFixes: Bool
    // Per-game compatibility overrides (-1 = use global). Driven by the generic
    // per-game INI helper; -1 clears the per-game key so the global value applies.
    @State private var perGameFixes: [String: Int]
    @State private var perGameAAT: Int
    @State private var perGameTextureInsideRt: Int
    @State private var perGameRenderer: Int
    @State private var perGameFXAA: Int
    @State private var perGameShadeBoost: Int
    @State private var perGameTVShader: Int
    @State private var perGameCASMode: Int
    @State private var perGameMaxAnisotropy: Int
    @State private var perGameCASSharpness: Int
    @State private var perGamePCRTCOffsets: Int
    @State private var perGameIntegerScaling: Int
    @State private var perGameSkipDupFrames: Int
    @State private var perGamePCRTCOverscan: Int
    @State private var perGamePCRTCAntiBlur: Int
    @State private var perGameDisableInterlaceOffset: Int
    @State private var perGameWidescreen: Int
    @State private var perGameNoInterlace: Int
    @State private var perGameShadeBoostBrightness: Int
    @State private var perGameShadeBoostContrast: Int
    @State private var perGameShadeBoostSaturation: Int
    @State private var perGameShadeBoostGamma: Int
    @State private var perGameDithering: Int
    @State private var perGameFastForwardVolume: Int
    @State private var perGameIOP: Int
    @State private var perGameVU0: Int
    @State private var perGameVU1: Int
    @State private var perGameHWDownloadMode: Int
    @State private var perGameCPUCLUT: Int
    @State private var perGameGPUTargetCLUT: Int
    @State private var perGameVsyncQueue: Int
    @State private var perGameLoadTextureReplacements: Int
    @State private var perGameLoadTextureReplacementsAsync: Int
    @State private var perGamePrecacheTextureReplacements: Int
    @State private var perGameSyncToHostRefresh: Int
    @State private var perGameBufferMS: Int
    @State private var perGameOutputLatencyMS: Int
    @State private var statusMessage: String?
    @State private var showCheatsManager = false
    @State private var showResetAllConfirmation = false
    @State private var showDiscardConfirmation = false
    @State private var savedFingerprint: String = ""
    @State private var landscapeCategory: PerGameSettingsCategory = .general

    init(
        game: ISOEntry,
        preloadedSettings: [String: Any]? = nil,
        savesToRunningGame: Bool = false,
        onDone: (() -> Void)? = nil
    ) {
        self.game = game
        self.onDone = onDone
        self.savesToRunningGame = savesToRunningGame
        // The runtime caller passes settings it already loaded through a VM-safe path so
        // this view never re-scans the disc image during init while a game is running.
        let info = preloadedSettings ?? ARMSX2Bridge.gameSettings(forISO: game.bootName)
        _enabled = State(initialValue: Self.boolValue(info["enabled"], defaultValue: false))
        let inheritedVolume = Self.clampedVolume(Self.intValue(info["globalVolumePercent"], defaultValue: SettingsStore.defaultEmulatorVolumePercent))
        let loadedVolume = Self.clampedVolume(Self.intValue(info["volumePercent"], defaultValue: inheritedVolume))
        _globalVolumePercent = State(initialValue: inheritedVolume)
        _volumeOverride = State(initialValue: Self.boolValue(info["hasVolumeOverride"], defaultValue: false))
        _volumePercent = State(initialValue: loadedVolume)
        _padLayoutIdentity = State(initialValue: PadLayoutGameIdentity(
            serial: (info["serial"] as? String) ?? game.metadata["serial"],
            crc: (info["crc"] as? String) ?? game.metadata["crc"]
        ))
        _hasGameSettingsIdentity = State(initialValue: !PadLayoutGameIdentity.normalizedCRC((info["crc"] as? String) ?? game.metadata["crc"]).isEmpty)
        _upscaleMultiplier = State(initialValue: Self.floatValue(info["upscaleMultiplier"], defaultValue: 1.0))
        _aspectRatio = State(initialValue: Self.normalizedAspect(info["aspectRatio"] as? String))
        _textureFiltering = State(initialValue: Self.intValue(info["textureFiltering"], defaultValue: 2))
        _hardwareMipmapping = State(initialValue: Self.boolValue(info["hardwareMipmapping"], defaultValue: true))
        _blendingAccuracy = State(initialValue: Self.intValue(info["blendingAccuracy"], defaultValue: 1))
        _interlaceMode = State(initialValue: Self.intValue(info["interlaceMode"], defaultValue: 7))
        _trilinearFiltering = State(initialValue: Self.boolValue(info["hasTrilinearFilteringOverride"], defaultValue: false) ? Self.intValue(info["trilinearFiltering"], defaultValue: -1) : Self.trilinearUseGlobalSentinel)
        _halfPixelOffset = State(initialValue: Self.boolValue(info["hasHalfPixelOffsetOverride"], defaultValue: false) ? Self.intValue(info["halfPixelOffset"], defaultValue: 0) : Self.useGlobalSentinel)
        _roundSprite = State(initialValue: Self.boolValue(info["hasRoundSpriteOverride"], defaultValue: false) ? Self.intValue(info["roundSprite"], defaultValue: 0) : Self.useGlobalSentinel)
        _alignSpriteOverride = State(initialValue: Self.boolValue(info["hasAlignSpriteOverride"], defaultValue: false))
        _alignSprite = State(initialValue: Self.boolValue(info["alignSprite"], defaultValue: false))
        _mergeSpriteOverride = State(initialValue: Self.boolValue(info["hasMergeSpriteOverride"], defaultValue: false))
        _mergeSprite = State(initialValue: Self.boolValue(info["mergeSprite"], defaultValue: false))
        _wildArmsOffsetOverride = State(initialValue: Self.boolValue(info["hasWildArmsOffsetOverride"], defaultValue: false))
        _wildArmsOffset = State(initialValue: Self.boolValue(info["wildArmsOffset"], defaultValue: false))
        _textureOffsetXOverride = State(initialValue: Self.boolValue(info["hasTextureOffsetXOverride"], defaultValue: false))
        _textureOffsetX = State(initialValue: Self.clampedTextureOffset(Self.intValue(info["textureOffsetX"], defaultValue: 0)))
        _textureOffsetYOverride = State(initialValue: Self.boolValue(info["hasTextureOffsetYOverride"], defaultValue: false))
        _textureOffsetY = State(initialValue: Self.clampedTextureOffset(Self.intValue(info["textureOffsetY"], defaultValue: 0)))
        let hasSkipDrawStartOverride = Self.boolValue(info["hasSkipDrawStartOverride"], defaultValue: false)
        let hasSkipDrawEndOverride = Self.boolValue(info["hasSkipDrawEndOverride"], defaultValue: false)
        let initialSkipDrawStart = Self.clampedSkipDraw(Self.intValue(info["skipDrawStart"], defaultValue: 0))
        let initialSkipDrawEnd = Self.normalizedSkipDrawEnd(
            start: initialSkipDrawStart,
            end: Self.intValue(info["skipDrawEnd"], defaultValue: 0),
            startOverride: hasSkipDrawStartOverride,
            endOverride: hasSkipDrawEndOverride
        )
        _skipDrawStartOverride = State(initialValue: hasSkipDrawStartOverride)
        _skipDrawStart = State(initialValue: initialSkipDrawStart)
        _skipDrawEndOverride = State(initialValue: hasSkipDrawEndOverride)
        _skipDrawEnd = State(initialValue: initialSkipDrawEnd)
        _eeCoreType = State(initialValue: Self.intValue(info["eeCoreType"], defaultValue: 2))
        _mtvu = State(initialValue: Self.boolValue(info["mtvu"], defaultValue: true))
        let inheritedEECycleRate = Self.clampedEECycleRate(Self.intValue(info["globalEECycleRate"], defaultValue: 0))
        _globalEECycleRate = State(initialValue: inheritedEECycleRate)
        _eeCycleRate = State(initialValue: Self.boolValue(info["hasEECycleRateOverride"], defaultValue: false) ? Self.clampedEECycleRate(Self.intValue(info["eeCycleRate"], defaultValue: inheritedEECycleRate)) : Self.eeCycleRateUseGlobalSentinel)
        let inheritedEECycleSkip = SettingsStore.clampedCycleSkip(Int(ARMSX2Bridge.getINIInt("EmuCore/Speedhacks", key: "EECycleSkip", defaultValue: 0)))
        _globalEECycleSkip = State(initialValue: inheritedEECycleSkip)
        let inheritedFastBoot = Self.boolValue(info["globalFastBoot"], defaultValue: false)
        _globalFastBoot = State(initialValue: inheritedFastBoot)
        _fastBoot = State(initialValue: Self.boolValue(info["hasFastBootOverride"], defaultValue: false) ? (Self.boolValue(info["fastBoot"], defaultValue: inheritedFastBoot) ? Self.fastBootOn : Self.fastBootOff) : Self.fastBootUseGlobalSentinel)
        _enableCheats = State(initialValue: Self.boolValue(info["enableCheats"], defaultValue: false))
        _enablePatches = State(initialValue: Self.boolValue(info["enablePatches"], defaultValue: true))
        _enableGameFixes = State(initialValue: Self.boolValue(info["enableGameFixes"], defaultValue: true))
        _enableGameDBHardwareFixes = State(initialValue: Self.boolValue(info["enableGameDBHardwareFixes"], defaultValue: true))

        // Per-game compatibility overrides. The bridge preloads these from the game
        // settings INI in a single read, so the panel avoids dozens of repeated
        // per-game INI parses on open.
        var loadedFixes: [String: Int] = [:]
        let preloadedFixes = info["perGameFixes"] as? [String: Any] ?? [:]
        for option in SettingsStore.gameFixOptions {
            if let value = preloadedFixes[option.key] {
                loadedFixes[option.key] = Self.intValue(value, defaultValue: 0)
            } else {
                loadedFixes[option.key] = -1
            }
        }
        _perGameFixes = State(initialValue: loadedFixes)
        let hasPerGameAAT = Self.boolValue(info["hasPerGameAAT"], defaultValue: false)
        _perGameAAT = State(initialValue: hasPerGameAAT ? Self.intValue(info["perGameAAT"], defaultValue: 0) : -1)
        let hasPerGameTextureInsideRt = Self.boolValue(info["hasPerGameTextureInsideRt"], defaultValue: false)
        _perGameTextureInsideRt = State(initialValue: hasPerGameTextureInsideRt ? Self.intValue(info["perGameTextureInsideRt"], defaultValue: 0) : -1)
        let hasPerGameRenderer = Self.boolValue(info["hasPerGameRenderer"], defaultValue: false)
        _perGameRenderer = State(initialValue: hasPerGameRenderer ? Self.intValue(info["perGameRenderer"], defaultValue: 17) : -1)
        let hasPerGameFXAA = Self.boolValue(info["hasPerGameFXAA"], defaultValue: false)
        _perGameFXAA = State(initialValue: hasPerGameFXAA ? Self.intValue(info["perGameFXAA"], defaultValue: 0) : -1)
        let hasPerGameShadeBoost = Self.boolValue(info["hasPerGameShadeBoost"], defaultValue: false)
        _perGameShadeBoost = State(initialValue: hasPerGameShadeBoost ? Self.intValue(info["perGameShadeBoost"], defaultValue: 0) : -1)
        let hasPerGameTVShader = Self.boolValue(info["hasPerGameTVShader"], defaultValue: false)
        _perGameTVShader = State(initialValue: hasPerGameTVShader ? Self.intValue(info["perGameTVShader"], defaultValue: 0) : -1)
        let hasPerGameCASMode = Self.boolValue(info["hasPerGameCASMode"], defaultValue: false)
        _perGameCASMode = State(initialValue: hasPerGameCASMode ? Self.intValue(info["perGameCASMode"], defaultValue: 0) : -1)
        let hasPerGameMaxAnisotropy = Self.boolValue(info["hasPerGameMaxAnisotropy"], defaultValue: false)
        _perGameMaxAnisotropy = State(initialValue: hasPerGameMaxAnisotropy ? Self.intValue(info["perGameMaxAnisotropy"], defaultValue: 0) : -1)
        let hasPerGameCASSharpness = Self.boolValue(info["hasPerGameCASSharpness"], defaultValue: false)
        _perGameCASSharpness = State(initialValue: hasPerGameCASSharpness ? Self.intValue(info["perGameCASSharpness"], defaultValue: 50) : -1)
        let hasPerGamePCRTCOffsets = Self.boolValue(info["hasPerGamePCRTCOffsets"], defaultValue: false)
        _perGamePCRTCOffsets = State(initialValue: hasPerGamePCRTCOffsets ? Self.intValue(info["perGamePCRTCOffsets"], defaultValue: 0) : -1)
        let hasPerGameIntegerScaling = Self.boolValue(info["hasPerGameIntegerScaling"], defaultValue: false)
        _perGameIntegerScaling = State(initialValue: hasPerGameIntegerScaling ? Self.intValue(info["perGameIntegerScaling"], defaultValue: 0) : -1)
        let hasPerGameSkipDupFrames = Self.boolValue(info["hasPerGameSkipDupFrames"], defaultValue: false)
        _perGameSkipDupFrames = State(initialValue: hasPerGameSkipDupFrames ? Self.intValue(info["perGameSkipDupFrames"], defaultValue: 1) : -1)

        let hasPerGamePCRTCOverscan = Self.boolValue(info["hasPerGamePCRTCOverscan"], defaultValue: false)
        _perGamePCRTCOverscan = State(initialValue: hasPerGamePCRTCOverscan ? Self.intValue(info["perGamePCRTCOverscan"], defaultValue: 0) : -1)

        let hasPerGamePCRTCAntiBlur = Self.boolValue(info["hasPerGamePCRTCAntiBlur"], defaultValue: false)
        _perGamePCRTCAntiBlur = State(initialValue: hasPerGamePCRTCAntiBlur ? Self.intValue(info["perGamePCRTCAntiBlur"], defaultValue: 1) : -1)

        let hasPerGameDisableInterlaceOffset = Self.boolValue(info["hasPerGameDisableInterlaceOffset"], defaultValue: false)
        _perGameDisableInterlaceOffset = State(initialValue: hasPerGameDisableInterlaceOffset ? Self.intValue(info["perGameDisableInterlaceOffset"], defaultValue: 0) : -1)
        let perGameISO = game.bootName
        let useCurrent = savesToRunningGame
        _eeCycleSkip = State(initialValue: Self.loadedPerGameInt("EmuCore/Speedhacks", "EECycleSkip", globalDefault: Int32(inheritedEECycleSkip), useCurrent: useCurrent, iso: perGameISO))
        _perGameWidescreen = State(initialValue: Self.loadedPerGameBool("EmuCore", "EnableWideScreenPatches", useCurrent: useCurrent, iso: perGameISO))
        _perGameNoInterlace = State(initialValue: Self.loadedPerGameBool("EmuCore", "EnableNoInterlacingPatches", useCurrent: useCurrent, iso: perGameISO))
        _perGameShadeBoostBrightness = State(initialValue: Self.loadedPerGameInt("EmuCore/GS", "ShadeBoost_Brightness", globalDefault: 50, useCurrent: useCurrent, iso: perGameISO))
        _perGameShadeBoostContrast = State(initialValue: Self.loadedPerGameInt("EmuCore/GS", "ShadeBoost_Contrast", globalDefault: 50, useCurrent: useCurrent, iso: perGameISO))
        _perGameShadeBoostSaturation = State(initialValue: Self.loadedPerGameInt("EmuCore/GS", "ShadeBoost_Saturation", globalDefault: 50, useCurrent: useCurrent, iso: perGameISO))
        _perGameShadeBoostGamma = State(initialValue: Self.loadedPerGameInt("EmuCore/GS", "ShadeBoost_Gamma", globalDefault: 50, useCurrent: useCurrent, iso: perGameISO))
        _perGameDithering = State(initialValue: Self.loadedPerGameInt("EmuCore/GS", "dithering_ps2", globalDefault: 2, useCurrent: useCurrent, iso: perGameISO))
        _perGameFastForwardVolume = State(initialValue: Self.loadedPerGameInt("SPU2/Output", "FastForwardVolume", globalDefault: 100, useCurrent: useCurrent, iso: perGameISO))
        _perGameIOP = State(initialValue: Self.loadedPerGameBool("EmuCore/CPU/Recompiler", "EnableIOP", useCurrent: useCurrent, iso: perGameISO))
        _perGameVU0 = State(initialValue: Self.loadedPerGameBool("EmuCore/CPU/Recompiler", "EnableVU0", useCurrent: useCurrent, iso: perGameISO))
        _perGameVU1 = State(initialValue: Self.loadedPerGameBool("EmuCore/CPU/Recompiler", "EnableVU1", useCurrent: useCurrent, iso: perGameISO))
        _perGameHWDownloadMode = State(initialValue: Self.loadedPerGameInt("EmuCore/GS", "HWDownloadMode", globalDefault: 0, useCurrent: useCurrent, iso: perGameISO))
        _perGameCPUCLUT = State(initialValue: Self.loadedPerGameInt("EmuCore/GS", "UserHacks_CPUCLUTRender", globalDefault: 0, useCurrent: useCurrent, iso: perGameISO))
        _perGameGPUTargetCLUT = State(initialValue: Self.loadedPerGameInt("EmuCore/GS", "UserHacks_GPUTargetCLUTMode", globalDefault: 0, useCurrent: useCurrent, iso: perGameISO))
        _perGameVsyncQueue = State(initialValue: Self.loadedPerGameInt("EmuCore/GS", "VsyncQueueSize", globalDefault: 8, useCurrent: useCurrent, iso: perGameISO))
        _perGameLoadTextureReplacements = State(initialValue: Self.loadedPerGameBool("EmuCore/GS", "LoadTextureReplacements", useCurrent: useCurrent, iso: perGameISO))
        _perGameLoadTextureReplacementsAsync = State(initialValue: Self.loadedPerGameBool("EmuCore/GS", "LoadTextureReplacementsAsync", useCurrent: useCurrent, iso: perGameISO))
        _perGamePrecacheTextureReplacements = State(initialValue: Self.loadedPerGameBool("EmuCore/GS", "PrecacheTextureReplacements", useCurrent: useCurrent, iso: perGameISO))
        _perGameSyncToHostRefresh = State(initialValue: Self.loadedPerGameBool("EmuCore/GS", "SyncToHostRefreshRate", useCurrent: useCurrent, iso: perGameISO))
        _perGameBufferMS = State(initialValue: Self.loadedPerGameInt("SPU2/Output", "BufferMS", globalDefault: 50, useCurrent: useCurrent, iso: perGameISO))
        _perGameOutputLatencyMS = State(initialValue: Self.loadedPerGameInt("SPU2/Output", "OutputLatencyMS", globalDefault: 20, useCurrent: useCurrent, iso: perGameISO))
        _savedFingerprint = State(initialValue: perGameFingerprint())
    }

    /// Encodes the current editable per-game state so Save can be gated on real changes.
    private func perGameFingerprint() -> String {
        let fixes = SettingsStore.gameFixOptions.map { "\($0.key):\(perGameFixes[$0.key] ?? -1)" }.joined(separator: ",")
        return "\(enabled)|\(upscaleMultiplier)|\(aspectRatio)|\(textureFiltering)|\(hardwareMipmapping)|\(blendingAccuracy)|\(interlaceMode)|\(trilinearFiltering)|\(halfPixelOffset)|\(roundSprite)|\(alignSpriteOverride)|\(alignSprite)|\(mergeSpriteOverride)|\(mergeSprite)|\(wildArmsOffsetOverride)|\(wildArmsOffset)|\(textureOffsetXOverride)|\(textureOffsetX)|\(textureOffsetYOverride)|\(textureOffsetY)|\(skipDrawStartOverride)|\(skipDrawStart)|\(skipDrawEndOverride)|\(skipDrawEnd)|\(volumeOverride)|\(volumePercent)|\(eeCoreType)|\(mtvu)|\(eeCycleRate)|\(eeCycleSkip)|\(fastBoot)|\(enableCheats)|\(enablePatches)|\(enableGameFixes)|\(enableGameDBHardwareFixes)|\(perGameAAT)|\(perGameTextureInsideRt)|\(perGameRenderer)|\(perGameFXAA)|\(perGameShadeBoost)|\(perGameTVShader)|\(perGameCASMode)|\(perGameMaxAnisotropy)|\(perGameCASSharpness)|\(perGamePCRTCOffsets)|\(perGameIntegerScaling)|\(perGameSkipDupFrames)|\(perGamePCRTCOverscan)|\(perGamePCRTCAntiBlur)|\(perGameDisableInterlaceOffset)|\(perGameWidescreen)|\(perGameNoInterlace)|\(perGameShadeBoostBrightness)|\(perGameShadeBoostContrast)|\(perGameShadeBoostSaturation)|\(perGameShadeBoostGamma)|\(perGameDithering)|\(perGameFastForwardVolume)|\(perGameIOP)|\(perGameVU0)|\(perGameVU1)|\(perGameHWDownloadMode)|\(perGameCPUCLUT)|\(perGameGPUTargetCLUT)|\(perGameVsyncQueue)|\(perGameLoadTextureReplacements)|\(perGameLoadTextureReplacementsAsync)|\(perGamePrecacheTextureReplacements)|\(perGameSyncToHostRefresh)|\(perGameBufferMS)|\(perGameOutputLatencyMS)|\(fixes)"
    }

    private var hasPendingChanges: Bool {
        perGameFingerprint() != savedFingerprint
    }

    /// Clears every per-game override by disabling the master toggle and saving; the
    /// save path deletes all per-game keys so the global values apply on next boot.
    private func resetAllOverrides() {
        enabled = false
        save()
    }

    /// Whether OPH Flag Hack is effectively on for this game: a per-game override of 1, or
    /// the global value when the per-game override is set to use-global (-1). Used to hide
    /// the higher-resolution OPH suggestion once OPH is in effect. Passed to GraphicsTab.
    private var ophFlagHackEffective: Bool {
        let perGame = perGameFixes["OPHFlagHack"] ?? -1
        if perGame == 1 { return true }
        if perGame == 0 { return false }
        return settings.gameFixEnabled("OPHFlagHack")
    }

    var body: some View {
        GeometryReader { geo in
            // Use the landscape workbench (category rail + detail pane) whenever the
            // overlay card is wider than it is tall. This covers both iPhone landscape
            // (short, wide card) and iPad landscape (large, wide card). The previous
            // `height < 500` guard kept iPad landscape on the portrait root form; that
            // guard is removed so iPads get the same rail/detail workbench as iPhone
            // landscape. Portrait cards (taller than wide) keep the NavigationStack form.
            let useCompactSettingsLayout = geo.size.width > geo.size.height
            VStack(spacing: 0) {
                settingsContent(useCompactLayout: useCompactSettingsLayout, availableWidth: geo.size.width)
                    .frame(maxHeight: .infinity)
                saveCancelFooter(compact: useCompactSettingsLayout)
            }
        }
        .background(OverlayFrostBackground())
        .preferredColorScheme(.dark)
        .tint(OverlayTheme.accent)
        .interactiveDismissDisabled(hasPendingChanges)
        .fullScreenCover(isPresented: $showPadLayoutEditor) {
            PadLayoutEditView(
                onDismiss: { showPadLayoutEditor = false },
                context: perGamePadLayoutEditorContext
            )
        }
        .fullScreenCover(isPresented: $showCheatsManager) {
            CheatsPatchesManagerView(
                isoName: game.bootName,
                gameTitle: game.name,
                launchContext: savesToRunningGame ? .inGame : .library
            )
        }
        .confirmationDialog(settings.localized("Reset all per-game overrides?"),
                            isPresented: $showResetAllConfirmation,
                            titleVisibility: .visible) {
            Button(settings.localized("Reset All"), role: .destructive) {
                resetAllOverrides()
            }
            Button(settings.localized("Cancel"), role: .cancel) {}
        } message: {
            Text(settings.localized("All per-game overrides for this title are removed; global settings apply on the next boot or reset."))
        }
        .confirmationDialog(settings.localized("Discard your changes?"),
                            isPresented: $showDiscardConfirmation,
                            titleVisibility: .visible) {
            Button(settings.localized("Discard Changes"), role: .destructive) {
                dismissPanel()
            }
            Button(settings.localized("Keep Editing"), role: .cancel) {}
        } message: {
            Text(settings.localized("You have unsaved per-game settings changes."))
        }
    }

    @ViewBuilder
    private func settingsContent(useCompactLayout: Bool, availableWidth: CGFloat = 0) -> some View {
        if useCompactLayout {
            landscapeSettingsSplit(availableWidth: availableWidth)
        } else {
            NavigationStack {
                rootForm
                    .navigationTitle(settings.localized("Per-Game Settings"))
                    .navigationBarTitleDisplayMode(.inline)
                    .toolbarBackground(OverlayTheme.shell, for: .navigationBar)
                    .toolbarBackground(.visible, for: .navigationBar)
            }
        }
    }

    @ViewBuilder
    private func landscapeSettingsSplit(availableWidth: CGFloat) -> some View {
        // Adaptive rail: wide enough for the longest category label ("Fixes & Compatibility")
        // on large panels, clamped so narrow panels keep a usable detail pane. At .callout the
        // longest label needs ~190pt including its icon column and padding.
        let railWidth = min(200, max(168, availableWidth * 0.24))
        VStack(spacing: 0) {
            landscapeHeader
            OverlayTheme.separator.frame(height: 0.5)
            HStack(spacing: 0) {
                categoryRail
                    .frame(width: railWidth)
                    .background(OverlayTheme.card)
                OverlayTheme.separator.frame(width: 0.5)
                detailPane
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        .dynamicTypeSize(...DynamicTypeSize.accessibility3)
    }

    @ViewBuilder
    private var landscapeHeader: some View {
        HStack(spacing: 8) {
            Image(systemName: enabled ? "slider.horizontal.3" : "power")
                .font(.system(size: 18))
                .foregroundStyle(OverlayTheme.accent)
            Text(settings.localized("Per-Game Settings"))
                .font(.callout.weight(.semibold))
                .foregroundStyle(OverlayTheme.textPrimary)
            Text(displayName)
                .font(.caption)
                .foregroundStyle(OverlayTheme.textSecondary)
                .lineLimit(1)
                .truncationMode(.middle)
            Spacer(minLength: 8)
            if hasPendingChanges {
                Circle()
                    .fill(OverlayTheme.accent)
                    .frame(width: 7, height: 7)
            }
        }
        .padding(.horizontal, 16)
        .padding(.top, 6)
        .padding(.bottom, 3)
    }

    @ViewBuilder
    private var categoryRail: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 2) {
                ForEach(PerGameSettingsCategory.allCases) { category in
                    let selected = landscapeCategory == category
                    Button {
                        landscapeCategory = category
                    } label: {
                        HStack(spacing: 10) {
                            Image(systemName: category.systemImage)
                                .frame(width: 22)
                                .foregroundStyle(selected ? OverlayTheme.accent : OverlayTheme.textSecondary)
                            Text(settings.localized(category.titleKey))
                                .font(.callout)
                                .lineLimit(2)
                                .fixedSize(horizontal: false, vertical: true)
                                .foregroundStyle(selected ? OverlayTheme.textPrimary : OverlayTheme.textSecondary)
                            Spacer(minLength: 0)
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .frame(minHeight: 40)
                        .padding(.horizontal, 12)
                        .background(
                            selected ? OverlayTheme.cardElevated : Color.clear,
                            in: RoundedRectangle(cornerRadius: 8, style: .continuous)
                        )
                    }
                    .buttonStyle(.plain)
                }
            }
            .padding(.vertical, 8)
        }
    }

    @ViewBuilder
    private var detailPane: some View {
        detailContent(for: landscapeCategory)
            .pickerStyle(.menu)
    }

    private func detailContent(for category: PerGameSettingsCategory) -> AnyView {
        switch category {
        case .general:  return AnyView(generalTab)
        case .graphics: return AnyView(graphicsTab)
        case .audio:    return AnyView(audioTab)
        case .cpu:      return AnyView(cpuTab)
        case .pad:      return AnyView(padTab)
        case .fixes:    return AnyView(fixesTab)
        case .cheats:   return AnyView(cheatsTab)
        }
    }

    // MARK: - Tab construction

    private var generalTab: some View {
        GeneralTab(
            enabled: $enabled,
            hasGameSettingsIdentity: $hasGameSettingsIdentity,
            showResetAllConfirmation: $showResetAllConfirmation,
            statusMessage: $statusMessage,
            displayName: displayName,
            hasPendingChanges: hasPendingChanges,
            game: game,
            settings: settings
        )
    }

    private var graphicsTab: some View {
        GraphicsTab(
            enabled: $enabled,
            enableGameDBHardwareFixes: $enableGameDBHardwareFixes,
            trilinearUseGlobalSentinel: Self.trilinearUseGlobalSentinel,
            ophFlagHackEffective: ophFlagHackEffective,
            upscaleMultiplier: $upscaleMultiplier,
            aspectRatio: $aspectRatio,
            textureFiltering: $textureFiltering,
            hardwareMipmapping: $hardwareMipmapping,
            blendingAccuracy: $blendingAccuracy,
            interlaceMode: $interlaceMode,
            trilinearFiltering: $trilinearFiltering,
            halfPixelOffset: $halfPixelOffset,
            roundSprite: $roundSprite,
            alignSpriteOverride: $alignSpriteOverride,
            alignSprite: $alignSprite,
            mergeSpriteOverride: $mergeSpriteOverride,
            mergeSprite: $mergeSprite,
            wildArmsOffsetOverride: $wildArmsOffsetOverride,
            wildArmsOffset: $wildArmsOffset,
            textureOffsetXOverride: $textureOffsetXOverride,
            textureOffsetX: $textureOffsetX,
            textureOffsetYOverride: $textureOffsetYOverride,
            textureOffsetY: $textureOffsetY,
            skipDrawStartOverride: $skipDrawStartOverride,
            skipDrawStart: $skipDrawStart,
            skipDrawEndOverride: $skipDrawEndOverride,
            skipDrawEnd: $skipDrawEnd,
            perGameFXAA: $perGameFXAA,
            perGameShadeBoost: $perGameShadeBoost,
            perGameShadeBoostBrightness: $perGameShadeBoostBrightness,
            perGameShadeBoostContrast: $perGameShadeBoostContrast,
            perGameShadeBoostSaturation: $perGameShadeBoostSaturation,
            perGameShadeBoostGamma: $perGameShadeBoostGamma,
            perGameDithering: $perGameDithering,
            perGameTVShader: $perGameTVShader,
            perGameCASMode: $perGameCASMode,
            perGameMaxAnisotropy: $perGameMaxAnisotropy,
            perGameCASSharpness: $perGameCASSharpness,
            perGamePCRTCOffsets: $perGamePCRTCOffsets,
            perGameIntegerScaling: $perGameIntegerScaling,
            perGameSkipDupFrames: $perGameSkipDupFrames,
            perGamePCRTCOverscan: $perGamePCRTCOverscan,
            perGamePCRTCAntiBlur: $perGamePCRTCAntiBlur,
            perGameDisableInterlaceOffset: $perGameDisableInterlaceOffset,
            perGameHWDownloadMode: $perGameHWDownloadMode,
            perGameCPUCLUT: $perGameCPUCLUT,
            perGameGPUTargetCLUT: $perGameGPUTargetCLUT,
            perGameVsyncQueue: $perGameVsyncQueue,
            perGameLoadTextureReplacements: $perGameLoadTextureReplacements,
            perGameLoadTextureReplacementsAsync: $perGameLoadTextureReplacementsAsync,
            perGamePrecacheTextureReplacements: $perGamePrecacheTextureReplacements,
            perGameSyncToHostRefresh: $perGameSyncToHostRefresh,
            settings: settings
        )
    }

    private var audioTab: some View {
        AudioTab(
            enabled: $enabled,
            volumeOverride: $volumeOverride,
            volumePercent: $volumePercent,
            globalVolumePercent: $globalVolumePercent,
            perGameFastForwardVolume: $perGameFastForwardVolume,
            perGameBufferMS: $perGameBufferMS,
            perGameOutputLatencyMS: $perGameOutputLatencyMS,
            settings: settings
        )
    }

    private var cpuTab: some View {
        CPUTab(
            enabled: $enabled,
            eeCoreType: $eeCoreType,
            mtvu: $mtvu,
            eeCycleRate: $eeCycleRate,
            globalEECycleRate: $globalEECycleRate,
            eeCycleSkip: $eeCycleSkip,
            globalEECycleSkip: $globalEECycleSkip,
            fastBoot: $fastBoot,
            globalFastBoot: $globalFastBoot,
            perGameIOP: $perGameIOP,
            perGameVU0: $perGameVU0,
            perGameVU1: $perGameVU1,
            settings: settings,
            eeCycleRateUseGlobalSentinel: Self.eeCycleRateUseGlobalSentinel,
            fastBootUseGlobalSentinel: Self.fastBootUseGlobalSentinel,
            fastBootOff: Self.fastBootOff,
            fastBootOn: Self.fastBootOn
        )
    }

    private var padTab: some View {
        PadTab(
            padLayoutIdentity: $padLayoutIdentity,
            showPadLayoutEditor: $showPadLayoutEditor,
            layoutPresets: layoutPresets,
            skinLibrary: skinLibrary
        )
    }

    private var fixesTab: some View {
        FixesTab(
            enabled: $enabled,
            perGameRenderer: $perGameRenderer,
            perGameAAT: $perGameAAT,
            perGameTextureInsideRt: $perGameTextureInsideRt,
            perGameFixes: $perGameFixes,
            settings: settings
        )
    }

    private var cheatsTab: some View {
        CheatsTab(
            enabled: $enabled,
            enableGameFixes: $enableGameFixes,
            enableGameDBHardwareFixes: $enableGameDBHardwareFixes,
            perGameWidescreen: $perGameWidescreen,
            perGameNoInterlace: $perGameNoInterlace,
            showCheatsManager: $showCheatsManager,
            settings: settings
        )
    }

    private var rootForm: some View {
        Form {
            identitySection
            overridesSection
            categoryLinksSection
            statusSection
        }
        .scrollContentBackground(.hidden)
    }

    @ViewBuilder
    private var identitySection: some View {
        Section {
            HStack(spacing: 12) {
                Image(systemName: enabled ? "slider.horizontal.3" : "power")
                    .font(.title3)
                    .foregroundStyle(enabled ? Color.accentColor : Color.secondary)
                    .frame(width: 32, height: 32)
                    .background(Color.accentColor.opacity(enabled ? 0.14 : 0), in: Circle())

                VStack(alignment: .leading, spacing: 4) {
                    Text(displayName)
                        .font(.headline)
                        .lineLimit(2)
                    if let serial = game.metadata["serial"], !serial.isEmpty {
                        Text("\(serial)  ·  CRC \(PadLayoutGameIdentity.normalizedCRC(game.metadata["crc"] ?? ""))")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .textSelection(.enabled)
                    }
                    Text(hasPendingChanges
                         ? settings.localized("Unsaved changes — Save to apply on next boot/reset.")
                         : settings.localized("No pending changes."))
                        .font(.caption)
                        .foregroundStyle(hasPendingChanges ? Color.accentColor : Color.secondary)
                }
            }
            .padding(.vertical, 4)
        }
    }

    @ViewBuilder
    private var overridesSection: some View {
        Section {
            Toggle(settings.localized("Use Per-Game Overrides"), isOn: $enabled)
            Text(settings.localized("Overrides are saved for this game only and apply on the next boot/reset of this title."))
                .font(.caption)
                .foregroundStyle(.secondary)
            if !hasGameSettingsIdentity {
                Text("Start this game once before saving its settings.")
                    .font(.caption)
                    .foregroundStyle(.orange)
            }
            Button(role: .destructive) {
                showResetAllConfirmation = true
            } label: {
                Label(settings.localized("Reset All Overrides"), systemImage: "arrow.counterclockwise")
            }
            .disabled(!hasGameSettingsIdentity)
        }
    }

    @ViewBuilder
    private var categoryLinksSection: some View {
        Section {
            NavigationLink {
                graphicsTab
            } label: {
                Label(settings.localized("Graphics"), systemImage: "paintbrush")
            }
            NavigationLink {
                audioTab
            } label: {
                Label(settings.localized("Audio"), systemImage: "speaker.wave.2")
            }
            NavigationLink {
                cpuTab
            } label: {
                Label(settings.localized("CPU & Speedhacks"), systemImage: "cpu")
            }
            NavigationLink {
                padTab
            } label: {
                Label(settings.localized("Virtual Pad"), systemImage: "gamecontroller")
            }
            NavigationLink {
                fixesTab
            } label: {
                Label(settings.localized("Fixes & Compatibility"), systemImage: "wrench.and.screwdriver")
            }
            NavigationLink {
                cheatsTab
            } label: {
                Label(settings.localized("Cheats & Patches"), systemImage: "rectangle.stack.badge.plus")
            }
        }
    }

    @ViewBuilder
    private var statusSection: some View {
        if let statusMessage {
            Section {
                Text(statusMessage)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private func saveCancelFooter(compact: Bool) -> some View {
        HStack(spacing: 12) {
            Button(settings.localized(hasPendingChanges ? "Cancel" : "Close")) {
                attemptCancel()
            }
            .buttonStyle(.bordered)
            .controlSize(compact ? .regular : .large)
            .frame(maxWidth: .infinity)

            Button(settings.localized("Save")) {
                save()
            }
            .buttonStyle(.borderedProminent)
            .controlSize(compact ? .regular : .large)
            .disabled(!hasGameSettingsIdentity || !hasPendingChanges)
            .frame(maxWidth: .infinity)
        }
        .padding(.horizontal, compact ? 16 : 20)
        .padding(.top, compact ? 6 : 10)
        .padding(.bottom, compact ? 8 : 16)
        .background(OverlayFrostBackground())
        .overlay(alignment: .top) {
            OverlayTheme.separator
                .frame(height: 0.5)
        }
    }

    /// Cancel/dismiss guard: confirm before discarding unsaved edits. Reads `hasPendingChanges`
    /// only — it does not alter Save/Cancel gating or the fingerprint logic.
    private func attemptCancel() {
        if hasPendingChanges {
            showDiscardConfirmation = true
        } else {
            dismissPanel()
        }
    }

    private func dismissPanel() {
        if let onDone {
            onDone()
        } else {
            dismiss()
        }
    }

    private var perGamePadLayoutEditorContext: PadLayoutEditorContext {
        let preset = layoutPresets.effectivePreset(for: padLayoutIdentity)
        let editablePresetID = padLayoutIdentity.flatMap { layoutPresets.presetID(for: $0) }
        return PadLayoutEditorContext(
            presetID: editablePresetID,
            gameIdentity: padLayoutIdentity,
            initialSnapshot: preset?.snapshot,
            skinDescriptor: layoutPresets.effectiveSkinDescriptor(for: padLayoutIdentity, using: skinLibrary)
        )
    }

    private var displayName: String {
        let name = ((game.name as NSString).deletingPathExtension as String).trimmingCharacters(in: .whitespacesAndNewlines)
        let serial = game.metadata["serial"]?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""

        if name.isEmpty {
            return serial.isEmpty ? settings.localized("Current Game") : serial
        }
        if serial.isEmpty {
            return name
        }
        return "\(name) - \(serial)"
    }

    // MARK: - Per-game INI helpers
    // Bridge the generic per-game INI accessor for the compatibility overrides.
    // `useCurrent` selects the VM-safe current-game variant (which live-applies);
    // otherwise the ISO variant writes the per-game file without applying.

    /// Reads a per-game int override; returns -1 ("use global") when no per-game key is set.
    private static func loadedPerGameInt(_ section: String, _ key: String, globalDefault: Int32, useCurrent: Bool, iso: String) -> Int {
        if useCurrent {
            guard ARMSX2Bridge.hasPerGameINIValueForCurrentGame(section, key: key) else { return -1 }
            return Int(ARMSX2Bridge.getPerGameINIIntForCurrentGame(section, key: key, defaultValue: globalDefault))
        }
        guard ARMSX2Bridge.hasPerGameINIValue(section, key: key, forISO: iso) else { return -1 }
        return Int(ARMSX2Bridge.getPerGameINIInt(section, key: key, defaultValue: globalDefault, forISO: iso))
    }

    /// Reads a per-game bool override; returns -1 ("use global"), 0 (off), or 1 (on).
    private static func loadedPerGameBool(_ section: String, _ key: String, useCurrent: Bool, iso: String) -> Int {
        if useCurrent {
            guard ARMSX2Bridge.hasPerGameINIValueForCurrentGame(section, key: key) else { return -1 }
            return ARMSX2Bridge.getPerGameINIBoolForCurrentGame(section, key: key, defaultValue: false) ? 1 : 0
        }
        guard ARMSX2Bridge.hasPerGameINIValue(section, key: key, forISO: iso) else { return -1 }
        return ARMSX2Bridge.getPerGameINIBool(section, key: key, defaultValue: false, forISO: iso) ? 1 : 0
    }

    private static func setPerGameBoolValue(_ section: String, _ key: String, _ value: Bool, useCurrent: Bool, iso: String) {
        if useCurrent {
            ARMSX2Bridge.setPerGameINIBoolForCurrentGame(section, key: key, value: value)
        } else {
            ARMSX2Bridge.setPerGameINIBool(section, key: key, value: value, forISO: iso)
        }
    }

    private static func setPerGameIntValue(_ section: String, _ key: String, _ value: Int, useCurrent: Bool, iso: String) {
        if useCurrent {
            ARMSX2Bridge.setPerGameINIIntForCurrentGame(section, key: key, value: Int32(value))
        } else {
            ARMSX2Bridge.setPerGameINIInt(section, key: key, value: Int32(value), forISO: iso)
        }
    }

    private static func clearPerGameValue(_ section: String, _ key: String, useCurrent: Bool, iso: String) {
        if useCurrent {
            ARMSX2Bridge.deletePerGameINIValueForCurrentGame(section, key: key)
        } else {
            ARMSX2Bridge.deletePerGameINIValue(section, key: key, forISO: iso)
        }
    }

    private func save() {
        guard hasGameSettingsIdentity else {
            statusMessage = "Start this game once before saving its settings."
            return
        }
        let normalizedSkipDraw = normalizedSkipDrawValues()
        if skipDrawStart != normalizedSkipDraw.start {
            skipDrawStart = normalizedSkipDraw.start
        }
        if skipDrawEnd != normalizedSkipDraw.end {
            skipDrawEnd = normalizedSkipDraw.end
        }

        if savesToRunningGame {
            ARMSX2Bridge.setGameSettingsForCurrentGame(
                enabled: enabled,
                upscaleMultiplier: upscaleMultiplier,
                aspectRatio: aspectRatio,
                textureFiltering: Int32(textureFiltering),
                hardwareMipmapping: hardwareMipmapping,
                blendingAccuracy: Int32(blendingAccuracy),
                interlaceMode: Int32(interlaceMode),
                trilinearFiltering: Int32(trilinearFiltering),
                halfPixelOffset: Int32(halfPixelOffset),
                roundSprite: Int32(roundSprite),
                alignSpriteOverride: alignSpriteOverride,
                alignSprite: alignSprite,
                mergeSpriteOverride: mergeSpriteOverride,
                mergeSprite: mergeSprite,
                wildArmsOffsetOverride: wildArmsOffsetOverride,
                wildArmsOffset: wildArmsOffset,
                textureOffsetXOverride: textureOffsetXOverride,
                textureOffsetX: Int32(textureOffsetX),
                textureOffsetYOverride: textureOffsetYOverride,
                textureOffsetY: Int32(textureOffsetY),
                skipDrawStartOverride: skipDrawStartOverride,
                skipDrawStart: Int32(normalizedSkipDraw.start),
                skipDrawEndOverride: skipDrawEndOverride,
                skipDrawEnd: Int32(normalizedSkipDraw.end),
                volumeOverride: enabled && volumeOverride,
                volumePercent: Int32(volumePercent),
                eeCoreType: Int32(eeCoreType),
                mtvu: mtvu,
                eeCycleRateOverride: enabled && eeCycleRate != Self.eeCycleRateUseGlobalSentinel,
                eeCycleRate: Int32(Self.clampedEECycleRate(eeCycleRate == Self.eeCycleRateUseGlobalSentinel ? globalEECycleRate : eeCycleRate)),
                fastBootOverride: enabled && fastBoot != Self.fastBootUseGlobalSentinel,
                fastBoot: fastBoot == Self.fastBootOn,
                enableCheats: enableCheats,
                enablePatches: enablePatches,
                enableGameFixes: enableGameFixes,
                enableGameDBHardwareFixes: enableGameDBHardwareFixes
            )
        } else {
            ARMSX2Bridge.setGameSettings(
                forISO: game.bootName,
                enabled: enabled,
                upscaleMultiplier: upscaleMultiplier,
                aspectRatio: aspectRatio,
                textureFiltering: Int32(textureFiltering),
                hardwareMipmapping: hardwareMipmapping,
                blendingAccuracy: Int32(blendingAccuracy),
                interlaceMode: Int32(interlaceMode),
                trilinearFiltering: Int32(trilinearFiltering),
                halfPixelOffset: Int32(halfPixelOffset),
                roundSprite: Int32(roundSprite),
                alignSpriteOverride: alignSpriteOverride,
                alignSprite: alignSprite,
                mergeSpriteOverride: mergeSpriteOverride,
                mergeSprite: mergeSprite,
                wildArmsOffsetOverride: wildArmsOffsetOverride,
                wildArmsOffset: wildArmsOffset,
                textureOffsetXOverride: textureOffsetXOverride,
                textureOffsetX: Int32(textureOffsetX),
                textureOffsetYOverride: textureOffsetYOverride,
                textureOffsetY: Int32(textureOffsetY),
                skipDrawStartOverride: skipDrawStartOverride,
                skipDrawStart: Int32(normalizedSkipDraw.start),
                skipDrawEndOverride: skipDrawEndOverride,
                skipDrawEnd: Int32(normalizedSkipDraw.end),
                volumeOverride: enabled && volumeOverride,
                volumePercent: Int32(volumePercent),
                eeCoreType: Int32(eeCoreType),
                mtvu: mtvu,
                eeCycleRateOverride: enabled && eeCycleRate != Self.eeCycleRateUseGlobalSentinel,
                eeCycleRate: Int32(Self.clampedEECycleRate(eeCycleRate == Self.eeCycleRateUseGlobalSentinel ? globalEECycleRate : eeCycleRate)),
                fastBootOverride: enabled && fastBoot != Self.fastBootUseGlobalSentinel,
                fastBoot: fastBoot == Self.fastBootOn,
                enableCheats: enableCheats,
                enablePatches: enablePatches,
                enableGameFixes: enableGameFixes,
                enableGameDBHardwareFixes: enableGameDBHardwareFixes
            )
        }
        savePerGameCompatibility()
        let applyMessage = savesToRunningGame ?
            settings.localized("Volume changes apply now; some settings need reset or relaunch.") :
            settings.localized("Reset or relaunch the game to apply.")
        statusMessage = enabled ? "\(settings.localized("Saved for")) \(game.metadata["serial"] ?? game.name). \(applyMessage)" : settings.localized("Per-game overrides cleared.")
        savedFingerprint = perGameFingerprint()
    }

    /// Write the per-game compatibility overrides (game fixes, accurate alpha test,
    /// texture-inside-RT) via the generic per-game INI helper. "Use global" (-1) and
    /// a disabled master toggle both clear the per-game key so the global value wins.
    private func savePerGameCompatibility() {
        let useCurrent = savesToRunningGame
        let iso = game.bootName
        for option in SettingsStore.gameFixOptions {
            let state = perGameFixes[option.key] ?? -1
            if enabled && state != -1 {
                Self.setPerGameBoolValue("EmuCore/Gamefixes", option.key, state == 1, useCurrent: useCurrent, iso: iso)
            } else {
                Self.clearPerGameValue("EmuCore/Gamefixes", option.key, useCurrent: useCurrent, iso: iso)
            }
        }
        if enabled && perGameAAT != -1 {
            Self.setPerGameBoolValue("EmuCore/GS", "HWAccurateAlphaTest", perGameAAT == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "HWAccurateAlphaTest", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameTextureInsideRt != -1 {
            Self.setPerGameIntValue("EmuCore/GS", "UserHacks_TextureInsideRt", perGameTextureInsideRt, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "UserHacks_TextureInsideRt", useCurrent: useCurrent, iso: iso)
        }
        // Renderer is a boot-time choice, so write the per-game file only and let it
        // take effect on the next boot rather than switching a running game live.
        let rendererIso = game.bootName
        if enabled && perGameRenderer != -1 {
            ARMSX2Bridge.setPerGameINIInt("EmuCore/GS", key: "Renderer", value: Int32(perGameRenderer), forISO: rendererIso)
        } else {
            ARMSX2Bridge.deletePerGameINIValue("EmuCore/GS", key: "Renderer", forISO: rendererIso)
        }
        if enabled && perGameFXAA != -1 {
            Self.setPerGameBoolValue("EmuCore/GS", "fxaa", perGameFXAA == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "fxaa", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameShadeBoost != -1 {
            Self.setPerGameBoolValue("EmuCore/GS", "ShadeBoost", perGameShadeBoost == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "ShadeBoost", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameTVShader != -1 {
            Self.setPerGameIntValue("EmuCore/GS", "TVShader", perGameTVShader, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "TVShader", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameCASMode != -1 {
            Self.setPerGameIntValue("EmuCore/GS", "CASMode", perGameCASMode, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "CASMode", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameMaxAnisotropy != -1 {
            Self.setPerGameIntValue("EmuCore/GS", "MaxAnisotropy", perGameMaxAnisotropy, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "MaxAnisotropy", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameCASSharpness != -1 {
            Self.setPerGameIntValue("EmuCore/GS", "CASSharpness", perGameCASSharpness, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "CASSharpness", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGamePCRTCOffsets != -1 {
            Self.setPerGameBoolValue("EmuCore/GS", "pcrtc_offsets", perGamePCRTCOffsets == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "pcrtc_offsets", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameIntegerScaling != -1 {
            Self.setPerGameBoolValue("EmuCore/GS", "IntegerScaling", perGameIntegerScaling == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "IntegerScaling", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameSkipDupFrames != -1 {
            Self.setPerGameBoolValue("EmuCore/GS", "SkipDuplicateFrames", perGameSkipDupFrames == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "SkipDuplicateFrames", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGamePCRTCOverscan != -1 {
            Self.setPerGameBoolValue("EmuCore/GS", "pcrtc_overscan", perGamePCRTCOverscan == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "pcrtc_overscan", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGamePCRTCAntiBlur != -1 {
            Self.setPerGameBoolValue("EmuCore/GS", "pcrtc_antiblur", perGamePCRTCAntiBlur == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "pcrtc_antiblur", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameDisableInterlaceOffset != -1 {
            Self.setPerGameBoolValue("EmuCore/GS", "disable_interlace_offset", perGameDisableInterlaceOffset == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "disable_interlace_offset", useCurrent: useCurrent, iso: iso)
        }
        // High-value per-game overrides added via the generic helper path.
        if enabled && perGameWidescreen != -1 {
            Self.setPerGameBoolValue("EmuCore", "EnableWideScreenPatches", perGameWidescreen == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore", "EnableWideScreenPatches", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameNoInterlace != -1 {
            Self.setPerGameBoolValue("EmuCore", "EnableNoInterlacingPatches", perGameNoInterlace == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore", "EnableNoInterlacingPatches", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameShadeBoostBrightness != -1 {
            Self.setPerGameIntValue("EmuCore/GS", "ShadeBoost_Brightness", perGameShadeBoostBrightness, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "ShadeBoost_Brightness", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameShadeBoostContrast != -1 {
            Self.setPerGameIntValue("EmuCore/GS", "ShadeBoost_Contrast", perGameShadeBoostContrast, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "ShadeBoost_Contrast", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameShadeBoostSaturation != -1 {
            Self.setPerGameIntValue("EmuCore/GS", "ShadeBoost_Saturation", perGameShadeBoostSaturation, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "ShadeBoost_Saturation", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameShadeBoostGamma != -1 {
            Self.setPerGameIntValue("EmuCore/GS", "ShadeBoost_Gamma", perGameShadeBoostGamma, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "ShadeBoost_Gamma", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameDithering != -1 {
            Self.setPerGameIntValue("EmuCore/GS", "dithering_ps2", perGameDithering, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "dithering_ps2", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameFastForwardVolume != -1 {
            Self.setPerGameIntValue("SPU2/Output", "FastForwardVolume", perGameFastForwardVolume, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("SPU2/Output", "FastForwardVolume", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameIOP != -1 {
            Self.setPerGameBoolValue("EmuCore/CPU/Recompiler", "EnableIOP", perGameIOP == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/CPU/Recompiler", "EnableIOP", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameVU0 != -1 {
            Self.setPerGameBoolValue("EmuCore/CPU/Recompiler", "EnableVU0", perGameVU0 == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/CPU/Recompiler", "EnableVU0", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameVU1 != -1 {
            Self.setPerGameBoolValue("EmuCore/CPU/Recompiler", "EnableVU1", perGameVU1 == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/CPU/Recompiler", "EnableVU1", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameHWDownloadMode != -1 {
            Self.setPerGameIntValue("EmuCore/GS", "HWDownloadMode", perGameHWDownloadMode, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "HWDownloadMode", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameCPUCLUT != -1 {
            Self.setPerGameIntValue("EmuCore/GS", "UserHacks_CPUCLUTRender", perGameCPUCLUT, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "UserHacks_CPUCLUTRender", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameGPUTargetCLUT != -1 {
            Self.setPerGameIntValue("EmuCore/GS", "UserHacks_GPUTargetCLUTMode", perGameGPUTargetCLUT, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "UserHacks_GPUTargetCLUTMode", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameVsyncQueue != -1 {
            Self.setPerGameIntValue("EmuCore/GS", "VsyncQueueSize", perGameVsyncQueue, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "VsyncQueueSize", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameLoadTextureReplacements != -1 {
            Self.setPerGameBoolValue("EmuCore/GS", "LoadTextureReplacements", perGameLoadTextureReplacements == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "LoadTextureReplacements", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameLoadTextureReplacementsAsync != -1 {
            Self.setPerGameBoolValue("EmuCore/GS", "LoadTextureReplacementsAsync", perGameLoadTextureReplacementsAsync == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "LoadTextureReplacementsAsync", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGamePrecacheTextureReplacements != -1 {
            Self.setPerGameBoolValue("EmuCore/GS", "PrecacheTextureReplacements", perGamePrecacheTextureReplacements == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "PrecacheTextureReplacements", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameSyncToHostRefresh != -1 {
            Self.setPerGameBoolValue("EmuCore/GS", "SyncToHostRefreshRate", perGameSyncToHostRefresh == 1, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/GS", "SyncToHostRefreshRate", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameBufferMS != -1 {
            Self.setPerGameIntValue("SPU2/Output", "BufferMS", perGameBufferMS, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("SPU2/Output", "BufferMS", useCurrent: useCurrent, iso: iso)
        }
        if enabled && perGameOutputLatencyMS != -1 {
            Self.setPerGameIntValue("SPU2/Output", "OutputLatencyMS", perGameOutputLatencyMS, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("SPU2/Output", "OutputLatencyMS", useCurrent: useCurrent, iso: iso)
        }
        if enabled && eeCycleSkip != -1 {
            Self.setPerGameIntValue("EmuCore/Speedhacks", "EECycleSkip", eeCycleSkip, useCurrent: useCurrent, iso: iso)
        } else {
            Self.clearPerGameValue("EmuCore/Speedhacks", "EECycleSkip", useCurrent: useCurrent, iso: iso)
        }
    }

    private func normalizedSkipDrawValues() -> (start: Int, end: Int) {
        let start = Self.clampedSkipDraw(skipDrawStart)
        let end = Self.normalizedSkipDrawEnd(
            start: start,
            end: skipDrawEnd,
            startOverride: skipDrawStartOverride,
            endOverride: skipDrawEndOverride
        )
        return (start, end)
    }

    private static func normalizedAspect(_ value: String?) -> String {
        switch value {
        case "Stretch", "4:3", "16:9", "10:7":
            return value ?? "Auto 4:3/3:2"
        default:
            return "Auto 4:3/3:2"
        }
    }

    private static func boolValue(_ value: Any?, defaultValue: Bool) -> Bool {
        if let bool = value as? Bool {
            return bool
        }
        if let number = value as? NSNumber {
            return number.boolValue
        }
        return defaultValue
    }

    private static func intValue(_ value: Any?, defaultValue: Int) -> Int {
        if let number = value as? NSNumber {
            return number.intValue
        }
        return defaultValue
    }

    private static func floatValue(_ value: Any?, defaultValue: Float) -> Float {
        if let number = value as? NSNumber {
            return number.floatValue
        }
        return defaultValue
    }

    private static func clampedTextureOffset(_ offset: Int) -> Int {
        min(max(offset, SettingsStore.textureOffsetRange.lowerBound), SettingsStore.textureOffsetRange.upperBound)
    }

    private static func clampedSkipDraw(_ value: Int) -> Int {
        min(max(value, SettingsStore.skipDrawRange.lowerBound), SettingsStore.skipDrawRange.upperBound)
    }

    private static func clampedVolume(_ value: Int) -> Int {
        SettingsStore.clampedEmulatorVolumePercent(value)
    }

    private static func clampedEECycleRate(_ value: Int) -> Int {
        min(max(value, -3), 3)
    }

    private static func normalizedSkipDrawEnd(start: Int, end: Int, startOverride: Bool, endOverride: Bool) -> Int {
        let clampedEnd = clampedSkipDraw(end)
        guard startOverride && endOverride else {
            return clampedEnd
        }
        return SettingsStore.normalizedSkipDrawEnd(start: start, end: clampedEnd)
    }
}
