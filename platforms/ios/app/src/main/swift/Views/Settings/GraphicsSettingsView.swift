// GraphicsSettingsView.swift — Renderer, upscale, filter, and display settings
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct GraphicsSettingsView: View {
    @State private var settings = SettingsStore.shared
    @State private var showShaderCacheClearConfirm = false
    @State private var shaderCacheResult: String?
    @State private var showShaderCacheResult = false

    private var manualAdvancedHacks: Bool {
        !settings.enableGameDBHardwareFixes
    }

    private var skipDrawStartBinding: Binding<Int> {
        Binding(
            get: { settings.skipDrawStart },
            set: { newValue in
                settings.skipDrawStart = min(max(newValue, SettingsStore.skipDrawRange.lowerBound), SettingsStore.skipDrawRange.upperBound)
                settings.skipDrawEnd = SettingsStore.normalizedSkipDrawEnd(start: settings.skipDrawStart, end: settings.skipDrawEnd)
            }
        )
    }

    private var skipDrawEndBinding: Binding<Int> {
        Binding(
            get: { settings.skipDrawEnd },
            set: { newValue in
                settings.skipDrawEnd = SettingsStore.normalizedSkipDrawEnd(start: settings.skipDrawStart, end: newValue)
            }
        )
    }

    var body: some View {
        Form {
            Section(settings.localized("Renderer")) {
                Picker(settings.localized("Renderer"), selection: $settings.renderer) {
                    Text(settings.localized("Metal (Hardware)")).tag(17)
#if !targetEnvironment(macCatalyst)
                    Text(settings.localized("Software")).tag(13)
                    Text(settings.localized("Null (No Output)")).tag(11)
#endif
                }
#if targetEnvironment(macCatalyst)
                Text(settings.localized("Metal is required for the Mac Catalyst build. Requires restart."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
#else
                Text(settings.localized("Metal is the supported iOS renderer. Software is slow but useful for debugging. Null disables rendering. Requires restart."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
#endif
#if !targetEnvironment(macCatalyst)
                if settings.renderer == 11 {
                    Text(settings.localized("Null renderer may show no video output or a black screen. It is mainly useful for testing. Switch back to Metal and restart if selected by mistake."))
                        .font(.caption)
                        .foregroundStyle(.orange)
                }
#endif

                Button(role: .destructive) {
                    showShaderCacheClearConfirm = true
                } label: {
                    Label(settings.localized("Clear Shader Cache"), systemImage: "square.slash")
                }
                Text(settings.localized("Removes cached GS/Metal shader and pipeline artifacts so they rebuild from scratch. Use this if visuals glitch after a settings change. Effects apply on the next frame or restart."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Section(settings.localized("Upscaling")) {
                Picker(settings.localized("Internal Resolution"), selection: $settings.upscaleMultiplier) {
                    Text(settings.localized("0.25x (Fastest)")).tag(Float(0.25))
                    Text("0.5x").tag(Float(0.5))
                    Text("0.75x").tag(Float(0.75))
                    Text(settings.localized("1x Native (512x448)")).tag(Float(1.0))
                    Text("2x (1024x896)").tag(Float(2.0))
                    Text("3x (1536x1344)").tag(Float(3.0))
                    Text("4x (2048x1792)").tag(Float(4.0))
                    Text("5x (2560x2240)").tag(Float(5.0))
                    Text("6x (3072x2688)").tag(Float(6.0))
                    Text("8x (4096x3584)").tag(Float(8.0))
                }
                Text(settings.localized("Lower values can help performance on heavy games. Higher values improve visual quality but reduce performance significantly. Applies immediately; the renderer may briefly stutter while it reinits."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                if settings.upscaleMultiplier >= 4.0 {
                    Text(settings.localized("4x and higher can cause poor performance, heat, stutter, or instability on iPhone and iPad."))
                        .font(.caption)
                        .foregroundStyle(.orange)
                }
            }

            if settings.isMetalFXAvailable {
                Section(settings.localized("Upscaler")) {
                    Picker(settings.localized("Spatial Upscaler"), selection: $settings.upscaler) {
                        Text(settings.localized("Off (Bilinear)")).tag(0)
                        Text(settings.localized("MetalFX Spatial")).tag(1)
                    }
                    Text(settings.localized(
                        "GPU-accelerated upscaling via MetalFX. Renders at the native PS2 "
                        + "resolution and upscales to the display for sharper visuals at "
                        + "lower cost than a higher internal resolution. Applies immediately."
                    ))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                }
            }

            Section(settings.localized("Filtering")) {
                Picker(settings.localized("Texture Filtering"), selection: $settings.textureFiltering) {
                    Text(settings.localized("Nearest (Pixelated)")).tag(0)
                    Text(settings.localized("Bilinear (Forced)")).tag(1)
                    Text(settings.localized("Bilinear (PS2 Default)")).tag(2)
                    Text(settings.localized("Bilinear (Forced excl. Sprite)")).tag(3)
                }

                Toggle(settings.localized("Hardware Mipmapping"), isOn: $settings.hardwareMipmapping)
                Text(settings.localized("Emulates PS2 texture mipmaps in the hardware renderer. Leave on by default; turn off only if a game has mipmap shimmer, stripes, or bad texture LOD behavior. Requires reset/relaunch for safest results."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Toggle("FXAA", isOn: $settings.fxaa)
                Text(settings.localized("Fast anti-aliasing. Smooths edges but may blur textures slightly."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Toggle(settings.localized("CAS Sharpening"), isOn: Binding(
                    get: { settings.casMode > 0 },
                    set: { settings.casMode = $0 ? 1 : 0 }
                ))
                if settings.casMode > 0 {
                    HStack {
                        Text(settings.localized("Sharpness"))
                        Slider(value: Binding(
                            get: { Float(settings.casSharpness) / 100.0 },
                            set: { settings.casSharpness = Int($0 * 100) }
                        ), in: 0...1, onEditingChanged: { editing in
                            if editing { settings.beginVisualSliderEdit() } else { settings.endVisualSliderEdit() }
                        })
                        Text("\(settings.casSharpness)%")
                            .font(.caption)
                            .frame(width: 40)
                    }
                }
                Text(settings.localized("Contrast Adaptive Sharpening via Metal. Sharpens the image after rendering."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Section(settings.localized("Display")) {
                Picker(settings.localized("Deinterlace"), selection: $settings.interlaceMode) {
                    Text(settings.localized("None")).tag(0)
                    Text(settings.localized("Weave (TFF)")).tag(1)
                    Text(settings.localized("Weave (BFF)")).tag(2)
                    Text(settings.localized("Bob (TFF)")).tag(3)
                    Text(settings.localized("Bob (BFF)")).tag(4)
                    Text(settings.localized("Blend (TFF)")).tag(5)
                    Text(settings.localized("Blend (BFF)")).tag(6)
                    Text(settings.localized("Adaptive (Default)")).tag(7)
                }

                Picker(settings.localized("Aspect Ratio"), selection: $settings.aspectRatio) {
                    Text(settings.localized("Auto 4:3 / 3:2 (Default)")).tag(1)
                    Text("4:3").tag(2)
                    Text(settings.localized("16:9 (Widescreen)")).tag(3)
                    Text("10:7").tag(4)
                    Text(settings.localized("Stretch to Window")).tag(0)
                }
            }

            Section {
                Toggle(settings.localized("Screen Offsets"), isOn: $settings.pcrtcOffsets)
                Toggle(settings.localized("Show Overscan"), isOn: $settings.pcrtcOverscan)
                Toggle(settings.localized("Anti-Blur"), isOn: $settings.pcrtcAntiBlur)
                Toggle(settings.localized("Disable Interlace Offset"), isOn: $settings.disableInterlaceOffset)
                Toggle(settings.localized("Skip Duplicate Frames"), isOn: $settings.skipDuplicateFrames)
                Toggle(settings.localized("Integer Scaling"), isOn: $settings.integerScaling)
            } header: {
                Text(settings.localized("Screen / PCRTC"))
            } footer: {
                Text(settings.localized("Display output options. Most apply immediately."))
            }

            Section(settings.localized("Quality")) {
                Picker(settings.localized("Blending Accuracy"), selection: $settings.blendingAccuracy) {
                    Text(settings.localized("Minimum (Fast)")).tag(0)
                    Text(settings.localized("Basic (Default)")).tag(1)
                    Text(settings.localized("Medium")).tag(2)
                    Text(settings.localized("High")).tag(3)
                    Text(settings.localized("Full (Slow)")).tag(4)
                    Text(settings.localized("Ultra (Very Slow)")).tag(5)
                }
                Text(settings.localized("Higher accuracy fixes transparency issues but reduces performance."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Picker(settings.localized("Dithering"), selection: $settings.dithering) {
                    Text(settings.localized("Off")).tag(0)
                    Text(settings.localized("Unscaled")).tag(1)
                    Text(settings.localized("Scaled (Default)")).tag(2)
                }
            }

            Section {
                Toggle(settings.localized("Shade Boost"), isOn: $settings.shadeBoost)
                if settings.shadeBoost {
                    percentSlider("Brightness", value: $settings.shadeBoostBrightness)
                    percentSlider("Contrast", value: $settings.shadeBoostContrast)
                    percentSlider("Saturation", value: $settings.shadeBoostSaturation)
                    percentSlider("Gamma", value: $settings.shadeBoostGamma)
                }
            } header: {
                Text(settings.localized("Shade Boost"))
            } footer: {
                Text(settings.localized("Adjusts brightness, contrast, saturation, and gamma of the output image. Applies immediately."))
            }

            Section(settings.localized("Advanced Upscaling Hacks")) {
                Toggle(settings.localized("Manual Advanced Hacks"), isOn: Binding(
                    get: { manualAdvancedHacks },
                    set: { settings.enableGameDBHardwareFixes = !$0 }
                ))
                Text(settings.localized("GameDB Graphics Fixes are safest for most games. Manual Advanced Hacks disable those automatic graphics fixes and allow the sprite, texture-offset, and Skipdraw values below. Reset/relaunch may be needed."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Picker(settings.localized("Trilinear Filtering"), selection: $settings.trilinearFiltering) {
                    Text(settings.localized("Automatic / Default")).tag(-1)
                    Text(settings.localized("Off")).tag(0)
                    Text("PS2").tag(1)
                    Text(settings.localized("Forced")).tag(2)
                }
                if settings.trilinearFiltering != -1 {
                    Text(settings.localized("Non-automatic trilinear filtering may break textures in some games."))
                        .font(.caption)
                        .foregroundStyle(.orange)
                }

                Picker(settings.localized("Half-pixel Offset"), selection: $settings.halfPixelOffset) {
                    Text(settings.localized("Off")).tag(0)
                    Text(settings.localized("Normal / Vertex")).tag(1)
                    Text(settings.localized("Special / Texture")).tag(2)
                    Text(settings.localized("Special / Texture Aggressive")).tag(3)
                    Text(settings.localized("Align to Native")).tag(4)
                    Text(settings.localized("Align to Native + Texture Offset")).tag(5)
                }
                .disabled(!manualAdvancedHacks)

                Picker(settings.localized("Round Sprite"), selection: $settings.roundSprite) {
                    Text(settings.localized("Off")).tag(0)
                    Text(settings.localized("Half")).tag(1)
                    Text(settings.localized("Full")).tag(2)
                }
                .disabled(!manualAdvancedHacks)

                Toggle(settings.localized("Align Sprite"), isOn: $settings.alignSprite)
                    .disabled(!manualAdvancedHacks)
                Toggle(settings.localized("Merge Sprite"), isOn: $settings.mergeSprite)
                    .disabled(!manualAdvancedHacks)
                Toggle(settings.localized("Wild Arms Offset"), isOn: $settings.wildArmsOffset)
                    .disabled(!manualAdvancedHacks)

                ClampedIntField(title: settings.localized("Texture Offset X"), value: $settings.textureOffsetX, range: SettingsStore.textureOffsetRange, isEnabled: manualAdvancedHacks)
                ClampedIntField(title: settings.localized("Texture Offset Y"), value: $settings.textureOffsetY, range: SettingsStore.textureOffsetRange, isEnabled: manualAdvancedHacks)
                Text(settings.localized("Texture offsets are advanced troubleshooting values. Type a value and clamp to range. Default is 0."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                ClampedIntField(title: settings.localized("Skipdraw Start"), value: skipDrawStartBinding, range: SettingsStore.skipDrawRange, isEnabled: manualAdvancedHacks)
                ClampedIntField(title: settings.localized("Skipdraw End"), value: skipDrawEndBinding, range: SettingsStore.skipDrawRange, isEnabled: manualAdvancedHacks)
                Text(settings.localized("For Skipdraw 1, use Start 1 and End 1. Changes apply after reset/relaunch."))
                    .font(.caption)
                    .foregroundStyle(.orange)
            }

            Section {
                Toggle(settings.localized("Accurate Alpha Test"), isOn: $settings.hwAccurateAlphaTest)
                Text(settings.localized("Improves alpha-test accuracy for shadows and decals. Some titles look better with this on."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                intPicker("Texture Inside RT", selection: $settings.textureInsideRt, options: [
                    ("Off", 0), ("Inside Targets", 1), ("Merge Targets", 2)
                ])
                intPicker("Limit 24-Bit Depth", selection: $settings.limit24BitDepth, options: [
                    ("Off", 0), ("Prioritise Upper Bits", 1), ("Prioritise Lower Bits", 2)
                ])
                intPicker("Native Scaling", selection: $settings.nativeScaling, options: [
                    ("Off", 0), ("Normal", 1), ("Aggressive", 2), ("Normal (Maintain Upscale)", 3), ("Aggressive (Maintain Upscale)", 4)
                ])
                intPicker("CPU CLUT Render", selection: $settings.cpuClutRender, options: [
                    ("Disabled", 0), ("Normal", 1), ("Aggressive", 2)
                ])
                intPicker("GPU Target CLUT", selection: $settings.gpuTargetClut, options: [
                    ("Off", 0), ("Enabled (Exact Match)", 1), ("Enabled (Inside Target)", 2)
                ])
                intPicker("Bilinear Upscale", selection: $settings.bilinearUpscaleHack, options: [
                    ("Automatic", 0), ("Force Bilinear", 1), ("Force Nearest", 2)
                ])
                ClampedIntField(title: settings.localized("CPU Sprite Render BW"), value: $settings.cpuSpriteRenderBw, range: 0...10)
                ClampedIntField(title: settings.localized("CPU Sprite Render Level"), value: $settings.cpuSpriteRenderLevel, range: 0...2)
                intPicker("Max Anisotropy", selection: $settings.maxAnisotropy, options: [
                    ("Off", 0), ("2x", 2), ("4x", 4), ("8x", 8), ("16x", 16)
                ])
                intPicker("Hardware Download Mode", selection: $settings.hardwareDownloadMode, options: [
                    ("Enabled", 0), ("Force Full", 1), ("No Readbacks", 2), ("Unsynchronized", 3), ("Disabled", 4)
                ])
                intPicker("TV/CRT Shader", selection: $settings.tvShader, options: [
                    ("Off", 0), ("Scanline", 1), ("Diagonal", 2), ("Tri", 3), ("Wave", 4), ("Lottes", 5), ("4xRGSS", 6), ("NxAGSS", 7)
                ])

                ForEach(SettingsStore.gsBoolHackOptions) { option in
                    Toggle(settings.localized(option.label), isOn: Binding(
                        get: { settings.gsBoolHackEnabled(option.key) },
                        set: { settings.setGSBoolHack(option.key, $0) }
                    ))
                }
            } header: {
                Text(settings.localized("Hardware Fixes"))
            } footer: {
                Text(settings.localized("These hardware fixes are for compatibility. Most games should use Automatic or Default values."))
            }

            Section(settings.localized("Texture Replacement")) {
                Toggle(settings.localized("Load Replacement Textures"), isOn: $settings.loadTextureReplacements)
                Text(settings.localized("Loads PNG or DDS texture packs from Documents/textures/[Game Serial]/replacements/. Texture packs use app storage and may be large. Requires restart."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Toggle(settings.localized("Async Loading"), isOn: $settings.loadTextureReplacementsAsync)
                    .disabled(!settings.loadTextureReplacements)
                Text(settings.localized("Loads replacement textures in the background to reduce boot stalls."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Toggle(settings.localized("Precache Textures"), isOn: $settings.precacheTextureReplacements)
                    .disabled(!settings.loadTextureReplacements)
                Text(settings.localized("Loads all replacements when the game starts. Faster in-game, but uses more RAM."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Picker(settings.localized("Texture Preloading"), selection: $settings.texturePreloading) {
                    Text(settings.localized("Off")).tag(0)
                    Text(settings.localized("Partial")).tag(1)
                    Text(settings.localized("Full")).tag(2)
                }
                Text(settings.localized("Core texture preloading mode. Full can improve replacement behavior but may increase memory use."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                if settings.loadTextureReplacements && (settings.precacheTextureReplacements || settings.texturePreloading > 0) {
                    Text(settings.localized("Large texture packs can use a lot of RAM when preload/precache is active and may cause stalls or crashes."))
                        .font(.caption)
                        .foregroundStyle(.orange)
                }
            }

            Section(settings.localized("Texture Dumping")) {
                Toggle(settings.localized("Dump Replaceable Textures"), isOn: $settings.dumpReplaceableTextures)
                Text(settings.localized("Writes discovered textures to Documents/textures/[Game Serial]/dumps/. This can heavily reduce performance and grow app storage quickly."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                if settings.dumpReplaceableTextures {
                    Text(settings.localized("Texture dumping can heavily slow games and create very large dump folders. Turn it off after collecting the textures you need."))
                        .font(.caption)
                        .foregroundStyle(.orange)
                }

                Toggle(settings.localized("Dump Mipmaps"), isOn: $settings.dumpReplaceableMipmaps)
                    .disabled(!settings.dumpReplaceableTextures)
                Toggle(settings.localized("Dump During FMV"), isOn: $settings.dumpTexturesWithFMVActive)
                    .disabled(!settings.dumpReplaceableTextures)
                Toggle(settings.localized("Dump Direct Textures"), isOn: $settings.dumpDirectTextures)
                    .disabled(!settings.dumpReplaceableTextures)
                Toggle(settings.localized("Dump Palette Textures"), isOn: $settings.dumpPaletteTextures)
                    .disabled(!settings.dumpReplaceableTextures)
            }

            Section {
                Button(settings.localized("Reset Graphics to Defaults")) {
                    settings.resetGraphicsDefaults()
                }
                .foregroundStyle(.red)
            }
        }
        .navigationTitle(settings.localized("Graphics"))
        .navigationBarTitleDisplayMode(.inline)
        .confirmationDialog(
            settings.localized("Clear Shader Cache?"),
            isPresented: $showShaderCacheClearConfirm,
            titleVisibility: .visible
        ) {
            Button(settings.localized("Clear"), role: .destructive) {
                clearShaderCache()
            }
            Button(settings.localized("Cancel"), role: .cancel) {}
        } message: {
            Text(settings.localized("This removes cached shader and GS artifacts. The renderer may briefly stutter as they rebuild."))
        }
        .alert(settings.localized("Shader Cache"), isPresented: $showShaderCacheResult) {
            Button(settings.localized("OK")) {}
        } message: {
            Text(settings.localized(shaderCacheResult ?? ""))
        }
    }

    /// Clears the GS/Metal shader and pipeline cache. The Metal renderer builds
    /// shaders in memory from the compiled metallib, so there is no dedicated
    /// on-disk shader directory; the GS-generated cache under the app's cache
    /// directory holds the rebuildable artifacts this targets.
    private func clearShaderCache() {
        let docsPath = ARMSX2Bridge.documentsDirectory()
        let cacheURL = URL(fileURLWithPath: (docsPath as NSString).appendingPathComponent("cache"), isDirectory: true)
        let fileManager = FileManager.default
        var isDirectory: ObjCBool = false
        guard fileManager.fileExists(atPath: cacheURL.path, isDirectory: &isDirectory), isDirectory.boolValue else {
            shaderCacheResult = "Shader cache is already empty."
            showShaderCacheResult = true
            return
        }
        do {
            for child in try fileManager.contentsOfDirectory(at: cacheURL, includingPropertiesForKeys: nil) {
                try? fileManager.removeItem(at: child)
            }
            shaderCacheResult = "Shader cache cleared."
        } catch {
            shaderCacheResult = "Could not fully clear the cache: \(error.localizedDescription)"
        }
        showShaderCacheResult = true
    }

    /// Labeled picker over an explicit (label, value) option list, used for the GS
    /// hardware-fix enums whose values may be non-contiguous (e.g. anisotropy).
    @ViewBuilder
    private func intPicker(_ title: String, selection: Binding<Int>, options: [(String, Int)]) -> some View {
        Picker(settings.localized(title), selection: selection) {
            ForEach(Array(options.enumerated()), id: \.offset) { _, option in
                Text(settings.localized(option.0)).tag(option.1)
            }
        }
    }

    /// Labeled 1–100 percent slider used by Shade Boost.
    @ViewBuilder
    private func percentSlider(_ title: String, value: Binding<Int>) -> some View {
        HStack {
            Text(settings.localized(title))
            Slider(value: Binding(
                get: { Double(value.wrappedValue) },
                set: { value.wrappedValue = Int($0.rounded()) }
            ), in: 1...100, onEditingChanged: { editing in
                if editing { settings.beginVisualSliderEdit() } else { settings.endVisualSliderEdit() }
            })
            Text("\(value.wrappedValue)%")
                .font(.caption.monospacedDigit())
                .frame(width: 44, alignment: .trailing)
        }
    }
}

/// A typeable integer field for advanced/manual hack values. Text is committed when
/// editing ends: valid input is clamped to `range`, and invalid input reverts to the
/// last good value so a bad string can never be written or crash the field.
struct ClampedIntField: View {
    let title: String
    @Binding var value: Int
    let range: ClosedRange<Int>
    var isEnabled: Bool = true

    @State private var text: String = ""
    @FocusState private var focused: Bool

    var body: some View {
        HStack {
            Text(title)
            Spacer()
            TextField("0", text: $text)
                .keyboardType(.numbersAndPunctuation)
                .multilineTextAlignment(.trailing)
                .frame(maxWidth: 110)
                .focused($focused)
                .disabled(!isEnabled)
        }
        .onAppear { text = String(value) }
        .onChange(of: value) { _, newValue in
            if !focused { text = String(newValue) }
        }
        .onChange(of: focused) { _, isFocused in
            if !isFocused { commit() }
        }
    }

    private func commit() {
        if let parsed = Int(text.trimmingCharacters(in: .whitespaces)) {
            value = min(max(parsed, range.lowerBound), range.upperBound)
        }
        text = String(value)
    }
}
