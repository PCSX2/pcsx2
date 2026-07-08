// GraphicsTab.swift — Per-game Graphics category tab.
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct GraphicsTab: View {
    // Master toggle + gating.
    @Binding var enabled: Bool
    @Binding var enableGameDBHardwareFixes: Bool
    let trilinearUseGlobalSentinel: Int
    let ophFlagHackEffective: Bool

    // Core graphics overrides.
    @Binding var upscaleMultiplier: Float
    @Binding var aspectRatio: String
    @Binding var textureFiltering: Int
    @Binding var hardwareMipmapping: Bool
    @Binding var blendingAccuracy: Int
    @Binding var interlaceMode: Int

    // Advanced upscaling hacks.
    @Binding var trilinearFiltering: Int
    @Binding var halfPixelOffset: Int
    @Binding var roundSprite: Int
    @Binding var alignSpriteOverride: Bool
    @Binding var alignSprite: Bool
    @Binding var mergeSpriteOverride: Bool
    @Binding var mergeSprite: Bool
    @Binding var wildArmsOffsetOverride: Bool
    @Binding var wildArmsOffset: Bool
    @Binding var textureOffsetXOverride: Bool
    @Binding var textureOffsetX: Int
    @Binding var textureOffsetYOverride: Bool
    @Binding var textureOffsetY: Int
    @Binding var skipDrawStartOverride: Bool
    @Binding var skipDrawStart: Int
    @Binding var skipDrawEndOverride: Bool
    @Binding var skipDrawEnd: Int

    // Per-game compatibility overrides surfaced on this tab.
    @Binding var perGameFXAA: Int
    @Binding var perGameShadeBoost: Int
    @Binding var perGameShadeBoostBrightness: Int
    @Binding var perGameShadeBoostContrast: Int
    @Binding var perGameShadeBoostSaturation: Int
    @Binding var perGameShadeBoostGamma: Int
    @Binding var perGameDithering: Int
    @Binding var perGameTVShader: Int
    @Binding var perGameCASMode: Int
    @Binding var perGameMaxAnisotropy: Int
    @Binding var perGameCASSharpness: Int
    @Binding var perGamePCRTCOffsets: Int
    @Binding var perGameIntegerScaling: Int
    @Binding var perGameSkipDupFrames: Int
    @Binding var perGamePCRTCOverscan: Int
    @Binding var perGamePCRTCAntiBlur: Int
    @Binding var perGameDisableInterlaceOffset: Int
    @Binding var perGameHWDownloadMode: Int
    @Binding var perGameCPUCLUT: Int
    @Binding var perGameGPUTargetCLUT: Int
    @Binding var perGameVsyncQueue: Int
    @Binding var perGameLoadTextureReplacements: Int
    @Binding var perGameLoadTextureReplacementsAsync: Int
    @Binding var perGamePrecacheTextureReplacements: Int
    @Binding var perGameSyncToHostRefresh: Int

    let settings: SettingsStore

    // MARK: Static option tables (moved from the panel)

    private struct PickerOption: Identifiable {
        let id: Int
        let title: String
    }

    private static let useGlobalSentinel = -1
    private static let trilinearUseGlobalSentinelLocal = Int(Int32.min)

    private static let deinterlaceOptions = [
        PickerOption(id: 0, title: "None"),
        PickerOption(id: 1, title: "Weave (TFF)"),
        PickerOption(id: 2, title: "Weave (BFF)"),
        PickerOption(id: 3, title: "Bob (TFF)"),
        PickerOption(id: 4, title: "Bob (BFF)"),
        PickerOption(id: 5, title: "Blend (TFF)"),
        PickerOption(id: 6, title: "Blend (BFF)"),
        PickerOption(id: 7, title: "Adaptive (Default)")
    ]
    private static let trilinearFilteringOptions = [
        PickerOption(id: trilinearUseGlobalSentinelLocal, title: "Use Global"),
        PickerOption(id: -1, title: "Automatic / Default"),
        PickerOption(id: 0, title: "Off"),
        PickerOption(id: 1, title: "PS2"),
        PickerOption(id: 2, title: "Forced")
    ]
    private static let halfPixelOffsetOptions = [
        PickerOption(id: useGlobalSentinel, title: "Use Global"),
        PickerOption(id: 0, title: "Off"),
        PickerOption(id: 1, title: "Normal / Vertex"),
        PickerOption(id: 2, title: "Special / Texture"),
        PickerOption(id: 3, title: "Special / Texture Aggressive"),
        PickerOption(id: 4, title: "Align to Native"),
        PickerOption(id: 5, title: "Align to Native + Texture Offset")
    ]
    private static let roundSpriteOptions = [
        PickerOption(id: useGlobalSentinel, title: "Use Global"),
        PickerOption(id: 0, title: "Off"),
        PickerOption(id: 1, title: "Half"),
        PickerOption(id: 2, title: "Full")
    ]

    private static let upscaleOptions: [(id: Float, title: String)] = [
        (0.25, "0.25x (Fastest)"),
        (0.5, "0.5x"),
        (0.75, "0.75x"),
        (1.0, "1x Native"),
        (2.0, "2x"),
        (3.0, "3x"),
        (4.0, "4x")
    ]
    private static let aspectRatioOptions: [(id: String, title: String)] = [
        ("Auto 4:3/3:2", "Auto 4:3 / 3:2"),
        ("4:3", "4:3"),
        ("16:9", "16:9"),
        ("10:7", "10:7"),
        ("Stretch", "Stretch")
    ]
    private static let textureFilteringOptionsEnum: [(id: Int, title: String)] = [
        (0, "Nearest"),
        (1, "Bilinear Forced"),
        (2, "Bilinear PS2 Default"),
        (3, "Bilinear excl. Sprite")
    ]
    private static let blendingAccuracyOptions: [(id: Int, title: String)] = [
        (0, "Minimum"),
        (1, "Basic"),
        (2, "Medium"),
        (3, "High"),
        (4, "Full"),
        (5, "Ultra")
    ]

    var body: some View {
        PerGameTab(title: settings.localized("Graphics")) {
            graphicsContent
        }
    }

    @ViewBuilder
    private var graphicsContent: some View {
        Section(settings.localized("Graphics")) {
            EnumPicker(Self.upscaleOptions, selection: $upscaleMultiplier) {
                Text(settings.localized("Internal Resolution"))
            }
            .disabled(!enabled)

            if upscaleMultiplier > 1 && !ophFlagHackEffective {
                Text(settings.localized("Tip: OPH Flag Hack may help reduce slowdowns at higher resolutions."))
                    .font(.caption)
                    .foregroundStyle(OverlayTheme.warm)
            }

            EnumPicker(Self.aspectRatioOptions, selection: $aspectRatio) {
                Text(settings.localized("Aspect Ratio"))
            }
            .disabled(!enabled)

            EnumPicker(Self.textureFilteringOptionsEnum, selection: $textureFiltering) {
                Text(settings.localized("Texture Filtering"))
            }
            .disabled(!enabled)

            Toggle(settings.localized("Hardware Mipmapping"), isOn: $hardwareMipmapping)
                .disabled(!enabled)
            Text(settings.localized("Turn this off only for games with mipmap-related texture stripes, shimmer, or bad LOD. Reset/relaunch the game after changing it."))
                .font(.caption)
                .foregroundStyle(.secondary)

            EnumPicker(Self.blendingAccuracyOptions, selection: $blendingAccuracy) {
                Text(settings.localized("Blending Accuracy"))
            }
            .disabled(!enabled)

            Picker(settings.localized("Deinterlace"), selection: $interlaceMode) {
                ForEach(Self.deinterlaceOptions) { option in
                    Text(settings.localized(option.title)).tag(option.id)
                }
            }
            .disabled(!enabled)

            Picker(settings.localized("FXAA"), selection: $perGameFXAA) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("On")).tag(1)
            }
            .disabled(!enabled)

            Picker(settings.localized("Shade Boost"), selection: $perGameShadeBoost) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("On")).tag(1)
            }
            .disabled(!enabled)
            shadeBoostSlider(settings.localized("Shade Boost Brightness"), value: $perGameShadeBoostBrightness)
                .disabled(!enabled)
            shadeBoostSlider(settings.localized("Shade Boost Contrast"), value: $perGameShadeBoostContrast)
                .disabled(!enabled)
            shadeBoostSlider(settings.localized("Shade Boost Saturation"), value: $perGameShadeBoostSaturation)
                .disabled(!enabled)
            shadeBoostSlider(settings.localized("Shade Boost Gamma"), value: $perGameShadeBoostGamma)
                .disabled(!enabled)
            Picker(settings.localized("Dithering"), selection: $perGameDithering) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("Unscaled")).tag(1)
                Text(settings.localized("Scaled")).tag(2)
            }
            .disabled(!enabled)

            Picker(settings.localized("TV/CRT Shader"), selection: $perGameTVShader) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("Scanline")).tag(1)
                Text(settings.localized("Diagonal")).tag(2)
                Text(settings.localized("Tri")).tag(3)
                Text(settings.localized("Wave")).tag(4)
                Text(settings.localized("Lottes")).tag(5)
            }
            .disabled(!enabled)
            Text(settings.localized("Scanline and CRT effects are subtle on high-resolution displays and are more visible at a lower Internal Resolution."))
                .font(.caption)
                .foregroundStyle(.secondary)

            Picker(settings.localized("CAS Sharpening"), selection: $perGameCASMode) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("On")).tag(1)
            }
            .disabled(!enabled)

            Picker(settings.localized("Max Anisotropy"), selection: $perGameMaxAnisotropy) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text("2x").tag(2)
                Text("4x").tag(4)
                Text("8x").tag(8)
                Text("16x").tag(16)
            }
            .disabled(!enabled)

            Picker(settings.localized("CAS Sharpness"), selection: $perGameCASSharpness) {
                Text(settings.localized("Use Global")).tag(-1)
                Text("0").tag(0)
                Text("25").tag(25)
                Text("50").tag(50)
                Text("75").tag(75)
                Text("100").tag(100)
            }
            .disabled(!enabled)

            Picker(settings.localized("Screen Offsets"), selection: $perGamePCRTCOffsets) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("On")).tag(1)
            }
            .disabled(!enabled)

            Picker(settings.localized("Integer Scaling"), selection: $perGameIntegerScaling) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("On")).tag(1)
            }
            .disabled(!enabled)

            Picker(settings.localized("Skip Duplicate Frames"), selection: $perGameSkipDupFrames) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("On")).tag(1)
            }
            .disabled(!enabled)

            Picker(settings.localized("Show Overscan"), selection: $perGamePCRTCOverscan) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("On")).tag(1)
            }
            .disabled(!enabled)

            Picker(settings.localized("Anti-Blur"), selection: $perGamePCRTCAntiBlur) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("On")).tag(1)
            }
            .disabled(!enabled)

            Picker(settings.localized("Disable Interlace Offset"), selection: $perGameDisableInterlaceOffset) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("On")).tag(1)
            }
            .disabled(!enabled)
        }

        Section(settings.localized("Advanced Upscaling Hacks")) {
            Text(settings.localized("Manual advanced hacks only apply when Use Per-Game Overrides is on and GameDB Graphics Fixes is off. Save, then reset or relaunch the game."))
                .font(.caption)
                .foregroundStyle(.secondary)

            if enabled && enableGameDBHardwareFixes {
                Text(settings.localized("GameDB Graphics Fixes is on, so manual advanced hacks are saved but ignored until it is turned off for this game."))
                    .font(.caption)
                    .foregroundStyle(.orange)
            }

            Picker(settings.localized("Trilinear Filtering"), selection: $trilinearFiltering) {
                ForEach(Self.trilinearFilteringOptions) { option in
                    Text(settings.localized(option.title)).tag(option.id)
                }
            }
            .disabled(!enabled)

            if trilinearFiltering != trilinearUseGlobalSentinel && trilinearFiltering != -1 {
                Text(settings.localized("Non-automatic trilinear filtering may break textures in some games."))
                    .font(.caption)
                    .foregroundStyle(.orange)
            }

            Picker(settings.localized("Half-pixel Offset"), selection: $halfPixelOffset) {
                ForEach(Self.halfPixelOffsetOptions) { option in
                    Text(settings.localized(option.title)).tag(option.id)
                }
            }
            .disabled(!manualAdvancedHacksEnabled)

            Picker(settings.localized("Round Sprite"), selection: $roundSprite) {
                ForEach(Self.roundSpriteOptions) { option in
                    Text(settings.localized(option.title)).tag(option.id)
                }
            }
            .disabled(!manualAdvancedHacksEnabled)

            Toggle(settings.localized("Override Align Sprite"), isOn: $alignSpriteOverride)
                .disabled(!manualAdvancedHacksEnabled)
            if alignSpriteOverride {
                Toggle(settings.localized("Align Sprite"), isOn: $alignSprite)
                    .disabled(!manualAdvancedHacksEnabled)
            }

            Toggle(settings.localized("Override Merge Sprite"), isOn: $mergeSpriteOverride)
                .disabled(!manualAdvancedHacksEnabled)
            if mergeSpriteOverride {
                Toggle(settings.localized("Merge Sprite"), isOn: $mergeSprite)
                    .disabled(!manualAdvancedHacksEnabled)
            }

            Toggle(settings.localized("Override Wild Arms Offset"), isOn: $wildArmsOffsetOverride)
                .disabled(!manualAdvancedHacksEnabled)
            if wildArmsOffsetOverride {
                Toggle(settings.localized("Wild Arms Offset"), isOn: $wildArmsOffset)
                    .disabled(!manualAdvancedHacksEnabled)
            }

            Toggle(settings.localized("Override Texture Offset X"), isOn: $textureOffsetXOverride)
                .disabled(!manualAdvancedHacksEnabled)
            if textureOffsetXOverride {
                ClampedIntField(title: settings.localized("Texture Offset X"), value: $textureOffsetX, range: SettingsStore.textureOffsetRange, isEnabled: manualAdvancedHacksEnabled)
            }

            Toggle(settings.localized("Override Texture Offset Y"), isOn: $textureOffsetYOverride)
                .disabled(!manualAdvancedHacksEnabled)
            if textureOffsetYOverride {
                ClampedIntField(title: settings.localized("Texture Offset Y"), value: $textureOffsetY, range: SettingsStore.textureOffsetRange, isEnabled: manualAdvancedHacksEnabled)
            }

            Toggle(settings.localized("Override Skipdraw Start"), isOn: $skipDrawStartOverride)
                .disabled(!manualAdvancedHacksEnabled)
            if skipDrawStartOverride {
                ClampedIntField(title: settings.localized("Skipdraw Start"), value: skipDrawStartBinding, range: SettingsStore.skipDrawRange, isEnabled: manualAdvancedHacksEnabled)
            }

            Toggle(settings.localized("Override Skipdraw End"), isOn: $skipDrawEndOverride)
                .disabled(!manualAdvancedHacksEnabled)
            if skipDrawEndOverride {
                ClampedIntField(title: settings.localized("Skipdraw End"), value: skipDrawEndBinding, range: SettingsStore.skipDrawRange, isEnabled: manualAdvancedHacksEnabled)
            }
            if skipDrawStartOverride || skipDrawEndOverride {
                Text(settings.localized("For Skipdraw 1, use Start 1 and End 1. Changes apply after reset/relaunch."))
                    .font(.caption)
                    .foregroundStyle(.orange)
            }
        }

        Section(settings.localized("Hardware Fixes & Display")) {
            Picker(settings.localized("Hardware Download Mode"), selection: $perGameHWDownloadMode) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Enabled")).tag(0)
                Text(settings.localized("Force Full")).tag(1)
                Text(settings.localized("No Readbacks")).tag(2)
                Text(settings.localized("Unsynchronized")).tag(3)
                Text(settings.localized("Disabled")).tag(4)
            }
            .disabled(!enabled)
            Picker(settings.localized("CPU CLUT Render"), selection: $perGameCPUCLUT) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Disabled")).tag(0)
                Text(settings.localized("Normal")).tag(1)
                Text(settings.localized("Aggressive")).tag(2)
            }
            .disabled(!enabled)
            Picker(settings.localized("GPU Target CLUT"), selection: $perGameGPUTargetCLUT) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("Enabled (Exact)")).tag(1)
                Text(settings.localized("Enabled (Inside Target)")).tag(2)
            }
            .disabled(!enabled)
            Picker(settings.localized("VSync Queue Size"), selection: $perGameVsyncQueue) {
                Text(settings.localized("Use Global")).tag(-1)
                ForEach([2, 3, 4, 5, 6, 8, 10, 12, 16], id: \.self) { Text("\($0)").tag($0) }
            }
            .disabled(!enabled)
            Picker(settings.localized("Sync to Host Refresh"), selection: $perGameSyncToHostRefresh) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("On")).tag(1)
            }
            .disabled(!enabled)
            Text(settings.localized("Sync to Host Refresh needs a restart to take effect."))
                .font(.caption)
                .foregroundStyle(.secondary)
        }

        Section(settings.localized("Texture Replacement")) {
            Picker(settings.localized("Load Replacement Textures"), selection: $perGameLoadTextureReplacements) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("On")).tag(1)
            }
            .disabled(!enabled)
            Picker(settings.localized("Async Loading"), selection: $perGameLoadTextureReplacementsAsync) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("On")).tag(1)
            }
            .disabled(!enabled)
            Picker(settings.localized("Precache Textures"), selection: $perGamePrecacheTextureReplacements) {
                Text(settings.localized("Use Global")).tag(-1)
                Text(settings.localized("Off")).tag(0)
                Text(settings.localized("On")).tag(1)
            }
            .disabled(!enabled)
            Text(settings.localized("Texture replacement needs a restart to take effect."))
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }

    private var manualAdvancedHacksEnabled: Bool {
        enabled && !enableGameDBHardwareFixes
    }

    private var skipDrawStartBinding: Binding<Int> {
        Binding(
            get: { skipDrawStart },
            set: { newValue in
                skipDrawStart = Self.clampedSkipDraw(newValue)
                normalizeSkipDrawRangeIfNeeded()
            }
        )
    }

    private var skipDrawEndBinding: Binding<Int> {
        Binding(
            get: { skipDrawEnd },
            set: { newValue in
                skipDrawEnd = Self.normalizedSkipDrawEnd(
                    start: skipDrawStart,
                    end: newValue,
                    startOverride: skipDrawStartOverride,
                    endOverride: skipDrawEndOverride
                )
            }
        )
    }

    private func normalizeSkipDrawRangeIfNeeded() {
        let normalized = normalizedSkipDrawValues()
        if skipDrawStart != normalized.start {
            skipDrawStart = normalized.start
        }
        if skipDrawEnd != normalized.end {
            skipDrawEnd = normalized.end
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

    private static func clampedSkipDraw(_ value: Int) -> Int {
        min(max(value, SettingsStore.skipDrawRange.lowerBound), SettingsStore.skipDrawRange.upperBound)
    }

    private static func normalizedSkipDrawEnd(start: Int, end: Int, startOverride: Bool, endOverride: Bool) -> Int {
        let clampedEnd = clampedSkipDraw(end)
        guard startOverride && endOverride else {
            return clampedEnd
        }
        return SettingsStore.normalizedSkipDrawEnd(start: start, end: clampedEnd)
    }

    /// A 1...100 Shade Boost parameter row. -1 means "Use Global": the slider is hidden
    /// and a button restores the per-game override at the inherited global default so the
    /// user can dial in any value (the previous picker only offered 25/50/75/100).
    @ViewBuilder
    private func shadeBoostSlider(_ title: String, value: Binding<Int>) -> some View {
        if value.wrappedValue == -1 {
            HStack {
                Text(title)
                Spacer()
                Text(settings.localized("Use Global"))
                    .foregroundStyle(.secondary)
                Button(settings.localized("Override")) {
                    value.wrappedValue = 50
                }
                .buttonStyle(.bordered)
                .controlSize(.small)
            }
        } else {
            HStack {
                Text(title)
                Slider(value: Binding(
                    get: { Double(value.wrappedValue) },
                    set: { value.wrappedValue = Int($0.rounded()) }
                ), in: 1...100)
                Text("\(value.wrappedValue)%")
                    .font(.caption.monospacedDigit())
                    .frame(width: 44, alignment: .trailing)
                Button(settings.localized("Global")) {
                    value.wrappedValue = -1
                }
                .buttonStyle(.bordered)
                .controlSize(.small)
            }
        }
    }
}
