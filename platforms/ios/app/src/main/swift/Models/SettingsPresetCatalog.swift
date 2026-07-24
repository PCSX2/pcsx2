// SettingsPresetCatalog.swift — Built-in settings and Dynamic Control presets
// SPDX-License-Identifier: GPL-3.0+

import Foundation

enum BuiltInSettingsPreset: String, CaseIterable, Identifiable {
    case defaultPreset = "Default"
    case ultraQuality = "Ultra Quality"
    case highQuality = "High Quality"
    case highQuality30FPS = "High Quality 30 FPS"
    case performance = "Performance"
    case ultraPerformance = "Ultra Performance"

    var id: String { rawValue }

    var summary: String {
        switch self {
        case .defaultPreset:
            return "Restores the graphics and emulation options managed by quality presets to the ARMSX2 defaults."
        case .ultraQuality:
            return "Uses 2x internal resolution with FXAA and CAS Sharpening for maximum image quality."
        case .highQuality:
            return "Uses native internal resolution with FXAA and CAS Sharpening."
        case .highQuality30FPS:
            return "Uses the High Quality configuration and enables OPH Flag Hack for demanding 30 FPS games."
        case .performance:
            return "Uses native internal resolution without FXAA or CAS Sharpening to reduce GPU load."
        case .ultraPerformance:
            return "Uses the Performance configuration with Emulation-Only Mode. Intended for low-end devices."
        }
    }

    var detail: String {
        switch self {
        case .defaultPreset:
            return "Default disables Fast Boot, PNACH Cheats, Widescreen Patches, Fast CDVD, FXAA, CAS Sharpening, OPH Flag Hack, Emulation-Only Mode, and backgrounds in Help and Settings; restores Internal Resolution to 1x Native, Aspect Ratio to Auto, and Queue Size to 8; and selects the White Colored Virtual Pad skin."
        case .ultraQuality:
            return "Ultra Quality enables Fast Boot, PNACH Cheats, Widescreen Patches, Fast CDVD, FXAA, CAS Sharpening, and backgrounds in Help and Settings; sets Internal Resolution to 2x (1024x896), Aspect Ratio to Stretch to Window, and Queue Size to 2. OPH Flag Hack and Emulation-Only Mode are disabled. The selected Virtual Pad skin is preserved."
        case .highQuality:
            return "High Quality uses the Ultra Quality settings, including backgrounds in Help and Settings, with Internal Resolution set to 1x Native (512x448). The selected Virtual Pad skin is preserved."
        case .highQuality30FPS:
            return "High Quality 30 FPS uses the High Quality graphics and emulation settings, enables OPH Flag Hack, and disables backgrounds in Help and Settings. It does not change the frame-limiter target or the selected Virtual Pad skin."
        case .performance:
            return "Performance uses the High Quality settings with FXAA, CAS Sharpening, and backgrounds in Help and Settings disabled. The selected Virtual Pad skin is preserved."
        case .ultraPerformance:
            return "Ultra Performance uses the Performance settings, disables backgrounds in Help and Settings, and enables Emulation-Only Mode. This preset is intended for low-end devices and requires an external controller when Virtual Control Layout unloading is enabled. The selected Virtual Pad skin is preserved."
        }
    }

    var preservesVirtualPadSkin: Bool {
        self != .defaultPreset
    }

    @MainActor
    func isActive(
        settings: SettingsStore = .shared,
        skinLibrary: VPadSkinLibraryStore = .shared
    ) -> Bool {
        let configuration = self.configuration
        let settingsMatch = settings.fastBoot == configuration.fastBoot &&
            settings.enableCheats == configuration.enableCheats &&
            settings.enableWidescreenPatches == configuration.enableWidescreenPatches &&
            settings.fastCDVD == configuration.fastCDVD &&
            settings.fxaa == configuration.fxaa &&
            (settings.casMode > 0) == configuration.casSharpening &&
            settings.aspectRatio == configuration.aspectRatio &&
            settings.vsyncQueueSize == configuration.vsyncQueueSize &&
            settings.upscaleMultiplier == configuration.upscaleMultiplier &&
            settings.gameFixEnabled("OPHFlagHack") == configuration.ophFlagHack &&
            settings.emulationOnlyModeEnabled == configuration.emulationOnlyMode &&
            settings.backgroundEnabledInHelp == configuration.showBackgroundInHelpAndSettings &&
            settings.backgroundEnabledInSettings == configuration.showBackgroundInHelpAndSettings
        guard settingsMatch else { return false }
        guard self == .defaultPreset else { return true }
        return skinLibrary.selectedSkinID == VirtualPadSkin.armsx2Refresh.descriptorID &&
            settings.virtualPadSkin == .armsx2Refresh
    }

    @MainActor
    func apply(
        settings: SettingsStore = .shared,
        skinLibrary: VPadSkinLibraryStore = .shared
    ) {
        let configuration = self.configuration
        settings.fastBoot = configuration.fastBoot
        settings.enableCheats = configuration.enableCheats
        settings.enableWidescreenPatches = configuration.enableWidescreenPatches
        settings.fastCDVD = configuration.fastCDVD
        settings.fxaa = configuration.fxaa
        settings.casMode = configuration.casSharpening ? 1 : 0
        settings.aspectRatio = configuration.aspectRatio
        settings.vsyncQueueSize = configuration.vsyncQueueSize
        settings.upscaleMultiplier = configuration.upscaleMultiplier
        settings.setGameFix("OPHFlagHack", configuration.ophFlagHack)
        settings.emulationOnlyModeEnabled = configuration.emulationOnlyMode
        settings.backgroundEnabledInHelp = configuration.showBackgroundInHelpAndSettings
        settings.backgroundEnabledInSettings = configuration.showBackgroundInHelpAndSettings
        if self == .defaultPreset {
            skinLibrary.selectSkin(id: VirtualPadSkin.armsx2Refresh.descriptorID)
            settings.virtualPadSkin = .armsx2Refresh
        }
    }

    private var configuration: Configuration {
        switch self {
        case .defaultPreset:
            return Configuration(
                fastBoot: false,
                enableCheats: false,
                enableWidescreenPatches: false,
                fastCDVD: false,
                fxaa: false,
                casSharpening: false,
                aspectRatio: 1,
                vsyncQueueSize: 8,
                upscaleMultiplier: 1,
                ophFlagHack: false,
                emulationOnlyMode: false
            )
        case .ultraQuality:
            var configuration = qualityConfiguration(upscaleMultiplier: 2)
            configuration.showBackgroundInHelpAndSettings = true
            return configuration
        case .highQuality:
            var configuration = qualityConfiguration(upscaleMultiplier: 1)
            configuration.showBackgroundInHelpAndSettings = true
            return configuration
        case .highQuality30FPS:
            var configuration = qualityConfiguration(upscaleMultiplier: 1)
            configuration.ophFlagHack = true
            return configuration
        case .performance:
            var configuration = qualityConfiguration(upscaleMultiplier: 1)
            configuration.fxaa = false
            configuration.casSharpening = false
            return configuration
        case .ultraPerformance:
            var configuration = configurationForPerformance
            configuration.emulationOnlyMode = true
            return configuration
        }
    }

    private var configurationForPerformance: Configuration {
        var configuration = qualityConfiguration(upscaleMultiplier: 1)
        configuration.fxaa = false
        configuration.casSharpening = false
        return configuration
    }

    private func qualityConfiguration(upscaleMultiplier: Float) -> Configuration {
        Configuration(
            fastBoot: true,
            enableCheats: true,
            enableWidescreenPatches: true,
            fastCDVD: true,
            fxaa: true,
            casSharpening: true,
            aspectRatio: 0,
            vsyncQueueSize: 2,
            upscaleMultiplier: upscaleMultiplier,
            ophFlagHack: false,
            emulationOnlyMode: false
        )
    }

    private struct Configuration {
        var fastBoot: Bool
        var enableCheats: Bool
        var enableWidescreenPatches: Bool
        var fastCDVD: Bool
        var fxaa: Bool
        var casSharpening: Bool
        var aspectRatio: Int
        var vsyncQueueSize: Int
        var upscaleMultiplier: Float
        var ophFlagHack: Bool
        var emulationOnlyMode: Bool
        var showBackgroundInHelpAndSettings = false
    }
}

enum BuiltInDynamicControlPreset: String, CaseIterable, Identifiable {
    case defaultPreset = "Default"
    case mgs3 = "MGS 3"

    var id: String { rawValue }

    var summary: String {
        switch self {
        case .defaultPreset:
            return "Restores the Dynamic Control switches managed by presets while preserving sensitivities and button assignments."
        case .mgs3:
            return "Enables Dynamic Thumbsticks, Swipe Camera, right-thumbstick actions, the aiming crosshair, and Double Tap to Hold Aim."
        }
    }

    @MainActor
    func isActive(settings: DynamicThumbstickSettings = .shared) -> Bool {
        switch self {
        case .defaultPreset:
            return settings.legacyThumbsticks &&
                !settings.dynamicThumbsticks &&
                !settings.swipeCamera &&
                !settings.rightThumbstickActionsEnabled &&
                !settings.dynamicCrosshairEnabled &&
                !settings.doubleTapToHoldAim
        case .mgs3:
            return settings.dynamicThumbsticks &&
                settings.swipeCamera &&
                settings.rightThumbstickActionsEnabled &&
                settings.dynamicCrosshairEnabled &&
                settings.doubleTapToHoldAim
        }
    }

    @MainActor
    func apply(settings: DynamicThumbstickSettings = .shared) {
        switch self {
        case .defaultPreset:
            settings.setLegacyThumbsticks(true)
            settings.swipeCamera = false
            settings.rightThumbstickActionsEnabled = false
            settings.dynamicCrosshairEnabled = false
            settings.setDoubleTapToHoldAim(false)
        case .mgs3:
            settings.setDynamicThumbsticks(true)
            settings.swipeCamera = true
            settings.rightThumbstickActionsEnabled = true
            settings.dynamicCrosshairEnabled = true
            settings.setDoubleTapToHoldAim(true)
        }
    }
}
