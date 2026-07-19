// DynamicSettings.swift — Dynamic background models and advanced controls
// SPDX-License-Identifier: GPL-3.0+

import Foundation
import SwiftUI
import UIKit

// MARK: - DynamicBackgroundSettings

enum DynamicParticleStyle: String, CaseIterable, Identifiable, Codable {
  case xmb3
  case xmbMart
  case ps1Dust
  case ps4Glow
  case ps5Drift
  case mixed

  var id: String { rawValue }

  var title: String {
    switch self {
    case .xmb3:
      return "XMB3 Sparkles"
    case .xmbMart:
      return "PlayStation 3 XMB by Mart"
    case .ps1Dust:
      return "PS1 Dust"
    case .ps4Glow:
      return "PS4 Glow"
    case .ps5Drift:
      return "PS5 Drift"
    case .mixed:
      return "Mixed"
    }
  }
}

struct DynamicBackgroundMotionSettings: Equatable, Codable {
  var multicolorAmbientSpeed = 1.0
  var lightSpeedMotionSpeed = 1.0
  var spatialRetroSpeed = 1.0
  var towersOrbsSpeed = 1.0
  var playStation2MenuSceneSpeed = 1.0
  var playStation2MenuFramesPerSecond = 15.0
  var playStation2MenuYawSpeed = 2.0
  var playStation2MenuRollSpeed = 6.0
  var playStation2MenuOrbitDegrees = 25.0
  var playStation2MenuColorsTowers = false
  var faceButtonsSpeed = 1.0
  var faceButtonsShowsLightBands = true
  var playStationPortableBlurSpeed = 1.0
  var playStation3SplinesSpeed = 1.0
  var playStation4ParticlesSpeed = 1.0
  var playStation4WavesSpeed = 1.0
  var playStationRibbonsSpeed = 1.0
  var playStationPortableBlur = PlayStationPortableBlurSettings()
  var playStation3Splines = PlayStation3SplinesSettings()
  var playStation4Particles = PlayStation4ParticlesSettings()
  var playStation4Waves = PlayStation4WavesSettings()
  var playStationRibbons = PlayStationRibbonsSettings()
}

struct DynamicParticleSettings: Equatable, Codable {
  var isEnabled = false
  var style: DynamicParticleStyle = .xmb3
  var amount = 1.0
  var speed = 1.0
  var speedDirection = 0.5
  var dispersion = 1.0
  var verticalSpread = 1.0
  var verticalDensity = 1.0
  var outerDispersion = 0.0
  var size = 1.0
  var brightness = 0.7
  var opacity = 1.0
  var depthVariation = 0.0
  var twinkle = 0.65
  var drift = 0.55
  var verticalLevel = 0.5
  var widensPortraitBackground = true
  var backgroundPreviewBeforeApplyingDuration = 0.3
  var backgroundPreviewAfterApplyingDuration = 0.7
  var disablesDarkPaletteEffects = false
  var paletteDarkEffectIntensity = 1.0
  var sharedPaletteGradientTilt = 0.0
  var sharedPaletteGradientOffsetX = 0.0
  var sharedPaletteGradientOffsetY = 0.0
  var sharedPaletteGradientWidth = 1.0
  var sharedPaletteGradientCurvature = 0.0
  var multiColorAnimationSpeed = 1.0
  var multiColorAnimationSmoothness = 0.85
  var multiColorAnimationSpread = 0.35
  var faceButtonsEnabled = false
  var faceButtonAmount = 1.0
  var faceButtonSpeed = 0.33
  var faceButtonDispersion = 1.0
  var faceButtonSize = 1.0
  var faceButtonOpacity = 0.86
  var faceButtonRotation = 1.0
  var faceButtonPulse = 0.75
  var playStation3XMB = PlayStation3XMBSettings()
  var backgrounds = DynamicBackgroundMotionSettings()
}

struct ThemeMultiColorSelection: Equatable, Codable {
  var isEnabled = false
  var animates = false
  var palettes: [ThemePalette] = [.blue, .cyan, .pink, .gold]

  private var activePalettes: [ThemePalette] {
    palettes.isEmpty ? [.blue] : palettes
  }

  // Enables multi-color mode and gives it a valid fallback palette.
  mutating func setEnabled(_ enabled: Bool, fallback: ThemePalette) {
    isEnabled = enabled
    if enabled {
      palettes = fallback.relatedMultiColorPalettes
    }
  }

  // Adds or removes a palette while keeping at least one multi-color selected.
  mutating func toggle(_ palette: ThemePalette, fallback: ThemePalette) {
    if let index = palettes.firstIndex(of: palette) {
      if palettes.count > 1 {
        palettes.remove(at: index)
      }
    } else {
      palettes.append(palette)
    }

    if palettes.isEmpty {
      palettes = [fallback]
    }
  }

  // Returns the numbered selection badge for a palette in multi-color mode.
  func selectionNumber(for palette: ThemePalette) -> Int? {
    guard let index = palettes.firstIndex(of: palette) else { return nil }
    return index + 1
  }

  // Resolves colors from the selected palette list with optional soft animation.
  func color(
    index: Int,
    time: TimeInterval,
    settings: DynamicParticleSettings
  ) -> Color {
    let selectedPalettes = activePalettes

    guard animates, selectedPalettes.count > 1 else {
      return selectedPalettes[index % selectedPalettes.count].animatedColor(
        index: index,
        time: 0
      )
    }

    let progress =
      time * max(0.02, settings.multiColorAnimationSpeed) / 8
      + Double(index) * settings.multiColorAnimationSpread
    let step = Int(floor(progress))
    let fraction = progress - floor(progress)
    let fromPalette = selectedPalettes[positiveModulo(index + step, selectedPalettes.count)]
    let toPalette = selectedPalettes[positiveModulo(index + step + 1, selectedPalettes.count)]
    let blend = easedBlend(
      fraction,
      smoothness: settings.multiColorAnimationSmoothness
    )

    return blendedColor(
      fromPalette.animatedColor(
        index: index,
        time: time * 0.12
      ),
      toPalette.animatedColor(
        index: index,
        time: time * 0.12
      ),
      amount: blend
    )
  }

  private func easedBlend(_ fraction: Double, smoothness: Double) -> Double {
    let clampedSmoothness = min(1, max(0, smoothness))
    let smooth = fraction * fraction * (3 - 2 * fraction)
    return fraction * (1 - clampedSmoothness) + smooth * clampedSmoothness
  }

  private func blendedColor(_ first: Color, _ second: Color, amount: Double) -> Color {
    let firstColor = UIColor(first)
    let secondColor = UIColor(second)
    var firstRed: CGFloat = 0
    var firstGreen: CGFloat = 0
    var firstBlue: CGFloat = 0
    var firstAlpha: CGFloat = 0
    var secondRed: CGFloat = 0
    var secondGreen: CGFloat = 0
    var secondBlue: CGFloat = 0
    var secondAlpha: CGFloat = 0

    guard
      firstColor.getRed(
        &firstRed,
        green: &firstGreen,
        blue: &firstBlue,
        alpha: &firstAlpha
      ),
      secondColor.getRed(
        &secondRed,
        green: &secondGreen,
        blue: &secondBlue,
        alpha: &secondAlpha
      )
    else {
      return amount < 0.5 ? first : second
    }

    let blend = min(1, max(0, amount))
    return Color(
      red: Double(firstRed + (secondRed - firstRed) * blend),
      green: Double(firstGreen + (secondGreen - firstGreen) * blend),
      blue: Double(firstBlue + (secondBlue - firstBlue) * blend),
      opacity: Double(firstAlpha + (secondAlpha - firstAlpha) * blend)
    )
  }

  private func positiveModulo(_ value: Int, _ divisor: Int) -> Int {
    ((value % divisor) + divisor) % divisor
  }
}

// MARK: - PlayStationBackgroundSettings

struct PlayStation3SplinesSettings: Equatable, Codable {
  var showsSplines = true
  var showsParticles = true
  var strandCount = 15.0
  var verticalPosition = 0.66
  var strandSpacing = 1.0
  var waveAmplitude = 1.0
  var detailAmplitude = 1.0
  var phaseSpread = 1.0
  var glowOpacity = 1.0
  var coreOpacity = 1.0
  var glowWidth = 1.0
  var coreWidth = 1.0
  var glowBlur = 1.0
  var particleCount = 150.0
  var particleSpeed = 1.0
  var particleSpread = 1.0
  var particleSize = 1.0
  var particleOpacity = 1.0
  var particleTwinkle = 1.0
  var backgroundIntensity = 1.0
  var vignetteIntensity = 1.0
}

struct PlayStation4ParticlesSettings: Equatable, Codable {
  var showsParticles = true
  var showsWaves = true
  var showsWaveGlow = true
  var showsWaveCores = true
  var particleCount = 150.0
  var particleSpeed = 1.0
  var particleVerticalPosition = 0.48
  var particleSpread = 1.0
  var particleSize = 1.0
  var particleOpacity = 1.0
  var particleTwinkle = 1.0
  var waveCount = 8.0
  var waveVerticalPosition = 0.48
  var waveSpacing = 1.0
  var waveAmplitude = 1.0
  var waveCurvature = 1.0
  var phaseSpread = 1.0
  var glowBlur = 18.0
  var glowWidth = 1.0
  var glowOpacity = 1.0
  var coreWidth = 1.0
  var coreOpacity = 1.0
  var backgroundIntensity = 1.0
  var vignetteIntensity = 1.0
}

struct PlayStation4WavesSettings: Equatable, Codable {
  var showsAmbientGlows = true
  var showsParticles = true
  var showsWaves = true
  var showsWaveGlow = true
  var showsWaveCores = true
  var particleCount = 38.0
  var particleSpeed = 1.0
  var particleSpread = 1.0
  var particleSize = 1.0
  var particleOpacity = 1.0
  var waveCount = 8.0
  var waveVerticalPosition = 0.48
  var waveSpacing = 1.0
  var waveAmplitude = 1.0
  var waveCurvature = 1.0
  var phaseSpread = 1.0
  var glowBlur = 18.0
  var glowWidth = 1.0
  var glowOpacity = 1.0
  var coreWidth = 1.0
  var coreOpacity = 1.0
  var ambientGlowScale = 1.0
  var ambientGlowIntensity = 1.0
  var vignetteIntensity = 1.0
}

struct PlayStationRibbonsSettings: Equatable, Codable {
  var showsPanels = true
  var showsPanelGlow = true
  var showsPanelEdges = true
  var showsParticles = true
  var panelCount = 6.0
  var panelVerticalPosition = 0.14
  var panelSpacing = 1.0
  var panelAmplitude = 1.0
  var panelThickness = 1.0
  var panelCurvature = 1.0
  var phaseSpread = 1.0
  var panelFillOpacity = 1.0
  var panelGlowOpacity = 1.0
  var panelGlowBlur = 1.0
  var panelEdgeOpacity = 1.0
  var panelEdgeWidth = 1.0
  var particleCount = 86.0
  var particleSpeed = 1.0
  var particleDrift = 1.0
  var particleVerticalSpread = 1.0
  var particleSize = 1.0
  var particleOpacity = 1.0
  var ambientGlowScale = 1.0
  var ambientGlowIntensity = 1.0
  var vignetteIntensity = 1.0
}

struct PlayStationPortableBlurSettings: Equatable, Codable {
  var showsRibbons = true
  var showsGlow = true
  var showsCores = true
  var alternatesDirection = true
  var ribbonCount = 7.0
  var verticalOffset = 0.0
  var waveAmplitude = 1.0
  var detailAmplitude = 1.0
  var phaseSpread = 1.0
  var broadWidth = 1.0
  var glowBlur = 1.0
  var glowOpacity = 1.0
  var coreWidth = 1.0
  var coreBlur = 1.0
  var coreOpacity = 1.0
  var backgroundIntensity = 1.0
  var ambientGlowScale = 1.0
  var ambientGlowIntensity = 1.0
  var vignetteIntensity = 0.0
}

// MARK: - PlayStation3XMBSettings

enum PlayStation3XMBGradientPreset: String, CaseIterable, Identifiable, Codable {
  case theme
  case original
  case januaryDay = "01_day"
  case januaryNight = "01_night"
  case februaryDay = "02_day"
  case februaryNight = "02_night"
  case marchDay = "03_day"
  case marchNight = "03_night"
  case aprilDay = "04_day"
  case aprilNight = "04_night"
  case mayDay = "05_day"
  case mayNight = "05_night"
  case juneDay = "06_day"
  case juneNight = "06_night"
  case julyDay = "07_day"
  case julyNight = "07_night"
  case augustDay = "08_day"
  case augustNight = "08_night"
  case septemberDay = "09_day"
  case septemberNight = "09_night"
  case octoberDay = "10_day"
  case octoberNight = "10_night"
  case novemberDay = "11_day"
  case novemberNight = "11_night"
  case decemberDay = "12_day"
  case decemberNight = "12_night"

  var id: String { rawValue }

  var title: String {
    switch self {
    case .theme:
      return "Shared Themes + Ribbons"
    case .original:
      return "Original (RGB Sliders)"
    case .januaryDay: return "01 (Day)"
    case .januaryNight: return "01 (Night)"
    case .februaryDay: return "02 (Day)"
    case .februaryNight: return "02 (Night)"
    case .marchDay: return "03 (Day)"
    case .marchNight: return "03 (Night)"
    case .aprilDay: return "04 (Day)"
    case .aprilNight: return "04 (Night)"
    case .mayDay: return "05 (Day)"
    case .mayNight: return "05 (Night)"
    case .juneDay: return "06 (Day)"
    case .juneNight: return "06 (Night)"
    case .julyDay: return "07 (Day)"
    case .julyNight: return "07 (Night)"
    case .augustDay: return "08 (Day)"
    case .augustNight: return "08 (Night)"
    case .septemberDay: return "09 (Day)"
    case .septemberNight: return "09 (Night)"
    case .octoberDay: return "10 (Day)"
    case .octoberNight: return "10 (Night)"
    case .novemberDay: return "11 (Day)"
    case .novemberNight: return "11 (Night)"
    case .decemberDay: return "12 (Day)"
    case .decemberNight: return "12 (Night)"
    }
  }
}

struct PlayStation3XMBSettings: Equatable, Codable {
  var gradientPreset: PlayStation3XMBGradientPreset = .theme

  var colorR = 37.0
  var colorG = 89.0
  var colorB = 179.0
  var gradientTopMul = 0.09
  var gradientBotMul = 0.62

  var particleCount = 600.0
  var particleOpacity = 0.75
  var particleSizeBase = 10.0
  var particleSizeVariance = 1.5
  var particleFlowSpeed = 0.18
  var particleVerticalSpread = 1.0
  var particleVerticalDensity = 1.0
  var particleOuterDispersion = 0.0
  var particleDepthVariation = 0.0
  var particlesFollowWaveY = true

  var flowSpeed = 0.18
  var tension = 0.12
  var damping = 0.0001
  var length = 0.306001
  var spacing = 407.658
  var timeStep = 1.0

  var bandAmplitude = 0.2
  var bandSecondaryFrequency = 7.0
  var bandSecondaryAmplitude = 0.025
  var travelSpeed1 = 0.25
  var travelAmplitude1 = 0.014
  var travelSpeed2 = 0.15
  var travelAmplitude2 = 0.008

  var perturbation = 0.0998587
  var perturbationScale = 0.07
  var waveCosineAmplitude = 0.09
  var waveBias = -0.1
  var waveHeightScale = 0.5
  var waveSoftClip = 0.22

  var reversePipelineBlend = 0.45
  var reverseDescriptorStrength = 0.7
  var reverseSyntheticDescriptorSeed = 1337.0
  var reverseSyntheticDescriptorMotion = 0.65
  var reverseKernelGain = 0.04
  var reverseNormalizeGain = 0.08
  var reverseKernelPhaseStep = 0.45
  var reverseIndexJitter = 0.006
  var reverseTemporalSmooth = 0.84

  var fresnelPower = 4.0
  var fresnelScale = 0.5
  var opacity = 0.7
  var brightness = 0.35
  var zDetailScale = 0.08

  var ffdScale1X = 5.67726
  var ffdScale1Y = 1.00077
  var ffdScale1Z = 1.0
  var ffdScale2X = 2.82755
  var ffdScale2Y = 1.27579
  var ffdScale2Z = 2.88782
  var ffdOffsetX = 0.0
  var ffdOffsetY = -0.469999
  var ffdOffsetZ = 0.0
  var ffdYAmplitude = 0.05
  var ffdZAmplitude = 0.06
}

// MARK: - DynamicAppearancePreferences

struct DynamicAppearancePreferences: Codable, Equatable {
  private static let storageKey = "ARMSX2iOSDynamicAppearancePreferences"
  private static let currentVersion = 1

  var version = currentVersion
  var dynamicBackground: DynamicBackgroundStyle
  var sharedPalette: ThemePalette
  var sharedCustomColor: SavedPaletteColor?
  var sharedMultiColor: ThemeMultiColorSelection
  var ribbonPalette: ThemePalette
  var ribbonCustomColor: SavedPaletteColor?
  var ribbonMultiColor: ThemeMultiColorSelection
  var particleSettings: DynamicParticleSettings
  var hasSelectedPlayStation3XMBByMart: Bool
  var isPlayStation3XMBPresetExplicit: Bool

  static let standard = DynamicAppearancePreferences(
    dynamicBackground: .playStation2Menu,
    sharedPalette: .xmbAzureHorizon,
    sharedCustomColor: nil,
    sharedMultiColor: ThemeMultiColorSelection(),
    ribbonPalette: .xmbAzureHorizon,
    ribbonCustomColor: nil,
    ribbonMultiColor: ThemeMultiColorSelection(),
    particleSettings: DynamicParticleSettings(),
    hasSelectedPlayStation3XMBByMart: false,
    isPlayStation3XMBPresetExplicit: false
  )

  static func load(from userDefaults: UserDefaults = .standard) -> Self? {
    guard
      let data = userDefaults.data(forKey: storageKey),
      let preferences = DynamicBackgroundCoding.decode(Self.self, from: data),
      preferences.version == currentVersion
    else {
      return nil
    }

    return preferences
  }

  func save(to userDefaults: UserDefaults = .standard) {
    guard let data = DynamicBackgroundCoding.encode(self) else { return }
    userDefaults.set(data, forKey: Self.storageKey)
  }
}

// MARK: - PlayStationBackgroundSettingsControls

struct DynamicSettingsSliderActivity: @unchecked Sendable {
  let update: (_ title: String, _ value: String, _ isEditing: Bool) -> Void
}

private struct DynamicSettingsSliderActivityKey: EnvironmentKey {
  static let defaultValue = DynamicSettingsSliderActivity { _, _, _ in }
}

extension EnvironmentValues {
  var dynamicSettingsSliderActivity: DynamicSettingsSliderActivity {
    get { self[DynamicSettingsSliderActivityKey.self] }
    set { self[DynamicSettingsSliderActivityKey.self] = newValue }
  }
}

struct BackgroundSettingsResetHeader: View {
  let action: () -> Void

  var body: some View {
    HStack {
      Text("Advanced controls")
        .font(.caption.weight(.semibold))
        .foregroundStyle(.white.opacity(0.62))
      Spacer()
      Button(action: action) {
        Label("Reset all", systemImage: "arrow.counterclockwise")
          .font(.caption.weight(.semibold))
      }
      .buttonStyle(.borderless)
    }
  }
}

struct DynamicSettingsResetButton: View {
  let title: String
  let action: () -> Void

  var body: some View {
    Button(action: action) {
      Image(systemName: "arrow.counterclockwise")
        .font(.caption.weight(.semibold))
        .frame(width: 28, height: 28)
        .contentShape(Rectangle())
    }
    .buttonStyle(.borderless)
    .foregroundStyle(.white.opacity(0.7))
    .accessibilityLabel("Reset \(title)")
  }
}

struct BackgroundControlSection<Content: View>: View {
  let title: String
  let content: Content

  init(_ title: String, @ViewBuilder content: () -> Content) {
    self.title = title
    self.content = content()
  }

  var body: some View {
    VStack(alignment: .leading, spacing: 10) {
      Label(title, systemImage: sectionIcon)
        .font(.caption.weight(.bold))
        .foregroundStyle(.white.opacity(0.78))
      content
    }
    .padding(.vertical, 4)
  }

  private var sectionIcon: String {
    switch title {
    case "Visibility": return "eye"
    case "Native particles": return "sparkles"
    case "Environment": return "circle.lefthalf.filled"
    default: return title.contains("material") ? "slider.horizontal.3" : "waveform.path"
    }
  }
}

struct DynamicSettingsValueSlider: View {
  let title: String
  @Binding var value: Double
  let range: ClosedRange<Double>
  let step: Double?
  let formattedValue: String
  let resetValue: Double?

  @Environment(\.dynamicSettingsSliderActivity) private var sliderActivity
  @State private var isEditing = false

  var body: some View {
    VStack(alignment: .leading, spacing: 6) {
      HStack {
        Text(title)
        Spacer()
        Text(formattedValue)
          .foregroundStyle(.white.opacity(0.62))

        if let resetValue {
          DynamicSettingsResetButton(title: title) {
            value = resetValue
          }
        }
      }
      .font(.caption.weight(.semibold))

      Group {
        if let step {
          Slider(value: $value, in: range, step: step)
        } else {
          Slider(value: $value, in: range)
        }
      }
      .simultaneousGesture(
        DragGesture(minimumDistance: 0)
          .onChanged { _ in
            guard !isEditing else { return }
            isEditing = true
            sliderActivity.update(title, formattedValue, true)
          }
          .onEnded { _ in
            guard isEditing else { return }
            isEditing = false
            sliderActivity.update(title, formattedValue, false)
          }
      )
      .onChange(of: value) { _, _ in
        guard isEditing else { return }
        sliderActivity.update(title, formattedValue, true)
      }
    }
  }
}

struct BackgroundControlToggle: View {
  let title: String
  @Binding var value: Bool
  let defaultValue: Bool

  var body: some View {
    HStack {
      Toggle(title, isOn: $value)
      DynamicSettingsResetButton(title: title) {
        value = defaultValue
      }
    }
    .font(.caption.weight(.semibold))
  }
}

@MainActor
func controlSlider(
  _ title: String,
  value: Binding<Double>,
  range: ClosedRange<Double>,
  defaultValue: Double,
  format: @escaping (Double) -> String
) -> some View {
  DynamicSettingsValueSlider(
    title: title,
    value: value,
    range: range,
    step: nil,
    formattedValue: format(value.wrappedValue),
    resetValue: defaultValue
  )
}

@MainActor
func settingResetButton(
  _ title: String,
  action: @escaping () -> Void
) -> some View {
  DynamicSettingsResetButton(title: title, action: action)
}

extension View {
  func dynamicSettingsCard() -> some View {
    padding(14)
      .background {
        RoundedRectangle(cornerRadius: 16, style: .continuous)
          .fill(.gray.opacity(0.16))
      }
      .overlay {
        RoundedRectangle(cornerRadius: 16, style: .continuous)
          .stroke(.white.opacity(0.09), lineWidth: 0.7)
      }
  }
}

enum DynamicSettingsExpansion {
  static func binding(
    storage: Binding<String>,
    identifier: String
  ) -> Binding<Bool> {
    Binding(
      get: { identifierSet(in: storage.wrappedValue).contains(identifier) },
      set: { isExpanded in
        var identifiers = identifierSet(in: storage.wrappedValue)
        if isExpanded {
          identifiers.insert(identifier)
        } else {
          identifiers.remove(identifier)
        }
        storage.wrappedValue = identifiers.sorted().joined(separator: "|")
      }
    )
  }

  private static func identifierSet(in storage: String) -> Set<String> {
    Set(storage.split(separator: "|").map(String.init))
  }
}

@MainActor
func controlToggle(
  _ title: String,
  value: Binding<Bool>,
  defaultValue: Bool
) -> some View {
  BackgroundControlToggle(
    title: title,
    value: value,
    defaultValue: defaultValue
  )
}

func count(_ value: Double) -> String {
  String(format: "%.0f", value)
}

func percent(_ value: Double) -> String {
  "\(Int(value * 100))%"
}

func signedPercent(_ value: Double) -> String {
  String(format: "%+.0f%%", value * 100)
}

func multiplier(_ value: Double) -> String {
  String(format: "%.2fx", value)
}

func points(_ value: Double) -> String {
  String(format: "%.1f pt", value)
}

// MARK: - DynamicBackgroundSettingsControls

struct DynamicBackgroundSettingsControls: View {
  @Binding var particleSettings: DynamicParticleSettings
  @Binding var isPlayStation3XMBPresetExplicit: Bool
  let dynamicBackground: DynamicBackgroundStyle

  @AppStorage("ARMSX2iOSDynamicExpandedNestedGroups")
  private var expandedDynamicSettingsGroups = ""
  private let defaults = DynamicBackgroundMotionSettings()

  var body: some View {
    settingsContent
      .onAppear(perform: expandCurrentBackgroundSettings)
      .onChange(of: dynamicBackground) { _, _ in
        expandCurrentBackgroundSettings()
      }
  }

  private var settingsContent: some View {
    VStack(alignment: .leading, spacing: 10) {
      if dynamicBackground == .playStation3XMBByMart {
        backgroundSettingGroup(.playStation3XMBByMart) {
          PlayStation3XMBMartSettingsControls(
            settings: $particleSettings.playStation3XMB,
            isPresetExplicit: $isPlayStation3XMBPresetExplicit
          )
        }
      }
      activeBackgroundSettingGroup(.multicolorAmbient) {
        backgroundSpeedSlider(
          value: $particleSettings.backgrounds.multicolorAmbientSpeed,
          defaultValue: defaults.multicolorAmbientSpeed
        )
      }
      activeBackgroundSettingGroup(.lightSpeed) {
        backgroundSpeedSlider(
          value: $particleSettings.backgrounds.lightSpeedMotionSpeed,
          defaultValue: defaults.lightSpeedMotionSpeed
        )
      }
      activeBackgroundSettingGroup(.spatialRetro) {
        backgroundSpeedSlider(
          value: $particleSettings.backgrounds.spatialRetroSpeed,
          defaultValue: defaults.spatialRetroSpeed
        )
      }
      activeBackgroundSettingGroup(.towersOrbs) {
        backgroundSpeedSlider(
          value: $particleSettings.backgrounds.towersOrbsSpeed,
          defaultValue: defaults.towersOrbsSpeed
        )
      }
      activeBackgroundSettingGroup(.playStation2Menu) {
        particleSlider(
          "FPS",
          value: $particleSettings.backgrounds.playStation2MenuFramesPerSecond,
          range: 15...120,
          step: 1,
          formattedValue:
            "\(Int(particleSettings.backgrounds.playStation2MenuFramesPerSecond)) FPS",
          resetValue: defaults.playStation2MenuFramesPerSecond
        )
        backgroundSpeedSlider(
          value: $particleSettings.backgrounds.playStation2MenuSceneSpeed,
          defaultValue: defaults.playStation2MenuSceneSpeed
        )
        particleSlider(
          "Orbit angle",
          value: $particleSettings.backgrounds.playStation2MenuOrbitDegrees,
          range: 0...45,
          formattedValue: String(
            format: "%.0f°",
            particleSettings.backgrounds.playStation2MenuOrbitDegrees
          ),
          resetValue: defaults.playStation2MenuOrbitDegrees
        )
        particleSlider(
          "Yaw speed",
          value: $particleSettings.backgrounds.playStation2MenuYawSpeed,
          range: 0...10,
          formattedValue: String(
            format: "%.1fx",
            particleSettings.backgrounds.playStation2MenuYawSpeed
          ),
          resetValue: defaults.playStation2MenuYawSpeed
        )
        particleSlider(
          "Roll speed",
          value: $particleSettings.backgrounds.playStation2MenuRollSpeed,
          range: 0...12,
          formattedValue: String(
            format: "%.1fx",
            particleSettings.backgrounds.playStation2MenuRollSpeed
          ),
          resetValue: defaults.playStation2MenuRollSpeed
        )
        HStack {
          Toggle(
            "Colour towers",
            isOn: $particleSettings.backgrounds.playStation2MenuColorsTowers
          )
          settingResetButton("Colour towers") {
            particleSettings.backgrounds.playStation2MenuColorsTowers =
              defaults.playStation2MenuColorsTowers
          }
        }
      }
      activeBackgroundSettingGroup(.faceButtons) {
        backgroundSpeedSlider(
          value: $particleSettings.backgrounds.faceButtonsSpeed,
          defaultValue: defaults.faceButtonsSpeed
        )
        HStack {
          Toggle(
            "Show line waves",
            isOn: $particleSettings.backgrounds.faceButtonsShowsLightBands
          )
          settingResetButton("Show Face Buttons line waves") {
            particleSettings.backgrounds.faceButtonsShowsLightBands =
              defaults.faceButtonsShowsLightBands
          }
        }
      }
      activeBackgroundSettingGroup(.playStationPortableBlur) {
        backgroundSpeedSlider(
          value: $particleSettings.backgrounds.playStationPortableBlurSpeed,
          defaultValue: defaults.playStationPortableBlurSpeed
        )
        PlayStationPortableBlurSettingsControls(
          settings: $particleSettings.backgrounds.playStationPortableBlur
        )
      }
      activeBackgroundSettingGroup(.playStation3Splines) {
        backgroundSpeedSlider(
          value: $particleSettings.backgrounds.playStation3SplinesSpeed,
          defaultValue: defaults.playStation3SplinesSpeed
        )
        PlayStation3SplinesSettingsControls(
          settings: $particleSettings.backgrounds.playStation3Splines
        )
      }
      activeBackgroundSettingGroup(.playStation4Particles) {
        backgroundSpeedSlider(
          value: $particleSettings.backgrounds.playStation4ParticlesSpeed,
          defaultValue: defaults.playStation4ParticlesSpeed
        )
        PlayStation4ParticlesSettingsControls(
          settings: $particleSettings.backgrounds.playStation4Particles
        )
      }
      activeBackgroundSettingGroup(.playStation4Waves) {
        backgroundSpeedSlider(
          value: $particleSettings.backgrounds.playStation4WavesSpeed,
          defaultValue: defaults.playStation4WavesSpeed
        )
        PlayStation4WavesSettingsControls(
          settings: $particleSettings.backgrounds.playStation4Waves
        )
      }
      activeBackgroundSettingGroup(.playStationRibbons) {
        backgroundSpeedSlider(
          value: $particleSettings.backgrounds.playStationRibbonsSpeed,
          defaultValue: defaults.playStationRibbonsSpeed
        )
        PlayStationRibbonsSettingsControls(
          settings: $particleSettings.backgrounds.playStationRibbons
        )
      }
    }
  }

  private func backgroundSettingGroup<Content: View>(
    _ background: DynamicBackgroundStyle,
    @ViewBuilder content: @escaping () -> Content
  ) -> some View {
    DisclosureGroup(
      isExpanded: DynamicSettingsExpansion.binding(
        storage: $expandedDynamicSettingsGroups,
        identifier: background.settingsExpansionIdentifier
      )
    ) {
      VStack(alignment: .leading, spacing: 12) {
        content()
      }
      .padding(.top, 10)
    } label: {
      Label(background.title, systemImage: background.systemImage)
        .font(.subheadline.weight(.semibold))
        .foregroundStyle(.white)
    }
  }

  @ViewBuilder
  private func activeBackgroundSettingGroup<Content: View>(
    _ background: DynamicBackgroundStyle,
    @ViewBuilder content: @escaping () -> Content
  ) -> some View {
    if background == dynamicBackground {
      backgroundSettingGroup(background, content: content)
    }
  }

  private func expandCurrentBackgroundSettings() {
    DynamicSettingsExpansion.binding(
      storage: $expandedDynamicSettingsGroups,
      identifier: dynamicBackground.settingsExpansionIdentifier
    ).wrappedValue = true
  }

  private func backgroundSpeedSlider(
    value: Binding<Double>,
    defaultValue: Double
  ) -> some View {
    particleSlider(
      "Motion speed",
      value: value,
      range: 0.1...4,
      formattedValue: String(format: "%.1fx", value.wrappedValue),
      resetValue: defaultValue
    )
  }

  private func particleSlider(
    _ title: String,
    value: Binding<Double>,
    range: ClosedRange<Double>,
    step: Double? = nil,
    formattedValue: String,
    resetValue: Double
  ) -> some View {
    DynamicSettingsValueSlider(
      title: title,
      value: value,
      range: range,
      step: step,
      formattedValue: formattedValue,
      resetValue: resetValue
    )
  }

}

// MARK: - DynamicParticleSettingsControls

struct DynamicParticleSettingsControls: View {
  @Binding var particleSettings: DynamicParticleSettings
  @Binding var isPlayStation3XMBPresetExplicit: Bool
  let dynamicBackground: DynamicBackgroundStyle
  let resetAllSettingsAndPalettes: () -> Void

  @AppStorage("ARMSX2iOSDynamicAddParticlesExpanded")
  private var showsAddParticlesSettings = true
  @AppStorage("ARMSX2iOSDynamicBackgroundsExpanded")
  private var showsBackgroundSettings = false
  @AppStorage("ARMSX2iOSDynamicMultiColorExpanded")
  private var showsMultiColorSettings = false
  @AppStorage("ARMSX2iOSDynamicFaceButtonsExpanded")
  private var showsFaceButtonSettings = false
  private let defaults = DynamicParticleSettings()

  var body: some View {
    particleSection
      .onAppear { showsBackgroundSettings = true }
      .onChange(of: dynamicBackground) { _, _ in
        showsBackgroundSettings = true
      }
  }

  private var particleSection: some View {
    VStack(alignment: .leading, spacing: 12) {
      dynamicSettingsGroup(
        "Add particles",
        isExpanded: $showsAddParticlesSettings
      ) {
        addParticlesSettings
      }

      dynamicSettingsGroup(
        "Dynamic Backgrounds Settings",
        isExpanded: $showsBackgroundSettings
      ) {
        DynamicBackgroundSettingsControls(
          particleSettings: $particleSettings,
          isPlayStation3XMBPresetExplicit: $isPlayStation3XMBPresetExplicit,
          dynamicBackground: dynamicBackground
        )
      }

      dynamicSettingsGroup(
        "Multi-color animation",
        isExpanded: $showsMultiColorSettings
      ) {
        multiColorAnimationSettings
      }

      dynamicSettingsGroup(
        "Add face buttons",
        isExpanded: $showsFaceButtonSettings
      ) {
        faceButtonSettings
      }

      HStack {
        Toggle("Widen theme in portrait", isOn: $particleSettings.widensPortraitBackground)
        settingResetButton("Widen theme in portrait") {
          particleSettings.widensPortraitBackground =
            defaults.widensPortraitBackground
        }
      }
      .font(.subheadline.weight(.semibold))
      .foregroundStyle(.white)
      .dynamicSettingsCard()

      particleSlider(
        "See background preview before applying",
        value: $particleSettings.backgroundPreviewBeforeApplyingDuration,
        range: 0.0...3.0,
        formattedValue: String(
          format: "%.1fs",
          particleSettings.backgroundPreviewBeforeApplyingDuration
        )
      )
      .dynamicSettingsCard()

      particleSlider(
        "See background preview after applying",
        value: $particleSettings.backgroundPreviewAfterApplyingDuration,
        range: 0.0...3.0,
        formattedValue: String(
          format: "%.1fs",
          particleSettings.backgroundPreviewAfterApplyingDuration
        )
      )
      .dynamicSettingsCard()

      Button(role: .destructive, action: resetAllSettingsAndPalettes) {
        Label("Reset Dynamic Background Settings", systemImage: "arrow.counterclockwise")
          .font(.subheadline.weight(.bold))
          .frame(maxWidth: .infinity)
          .frame(height: 44)
      }
      .buttonStyle(.plain)
      .foregroundStyle(.red)
      .glassSurface(
        tint: Color.red.opacity(0.12),
        interactive: true,
        cornerRadius: 14
      )
      .accessibilityHint("Resets all dynamic settings and selected palettes")
    }
  }

  private var addParticlesSettings: some View {
    VStack(alignment: .leading, spacing: 12) {
      HStack {
        Toggle("Enable particles", isOn: $particleSettings.isEnabled)
        settingResetButton("Add particles") {
          particleSettings.isEnabled = defaults.isEnabled
        }
      }

      HStack {
        Picker("Particle style", selection: $particleSettings.style) {
          ForEach(DynamicParticleStyle.allCases) { style in
            Text(style.title).tag(style)
          }
        }
        .pickerStyle(.menu)
        settingResetButton("Particle style") {
          particleSettings.style = defaults.style
        }
      }

      particleSlider(
        "Amount",
        value: $particleSettings.amount,
        range: 0.2...3.0,
        formattedValue: "\(Int(particleSettings.amount * 100))%"
      )
      particleSlider(
        "Speed",
        value: $particleSettings.speed,
        range: 0.1...3.0,
        formattedValue: String(format: "%.1fx", particleSettings.speed)
      )
      particleSlider(
        "Direction of speed",
        value: $particleSettings.speedDirection,
        range: -1.0...1.0,
        formattedValue: String(
          format: "%+.0f%%",
          particleSettings.speedDirection * 100
        )
      )
      particleSlider(
        "Dispersion",
        value: $particleSettings.dispersion,
        range: 0.25...1.8,
        formattedValue: "\(Int(particleSettings.dispersion * 100))%"
      )
      particleSlider(
        "Y-axis spread",
        value: $particleSettings.verticalSpread,
        range: 0.25...3.0,
        formattedValue: "\(Int(particleSettings.verticalSpread * 100))%"
      )
      particleSlider(
        "Y-axis density",
        value: $particleSettings.verticalDensity,
        range: 0.25...3.0,
        formattedValue: "\(Int(particleSettings.verticalDensity * 100))%"
      )
      particleSlider(
        "Outer top/bottom dispersion",
        value: $particleSettings.outerDispersion,
        range: 0.0...2.5,
        formattedValue: "\(Int(particleSettings.outerDispersion * 100))%"
      )
      particleSlider(
        "Vertical level",
        value: $particleSettings.verticalLevel,
        range: 0.18...0.82,
        formattedValue: "\(Int(particleSettings.verticalLevel * 100))%"
      )
      particleSlider(
        "Size",
        value: $particleSettings.size,
        range: 0.4...2.4,
        formattedValue: String(format: "%.1fx", particleSettings.size)
      )
      particleSlider(
        "Brightness",
        value: $particleSettings.brightness,
        range: 0.15...1.35,
        formattedValue: "\(Int(particleSettings.brightness * 100))%"
      )
      particleSlider(
        "Particle opacity",
        value: $particleSettings.opacity,
        range: 0.05...1.5,
        formattedValue: "\(Int(particleSettings.opacity * 100))%"
      )
      particleSlider(
        "Depth variation",
        value: $particleSettings.depthVariation,
        range: 0.0...1.5,
        formattedValue: "\(Int(particleSettings.depthVariation * 100))%"
      )
      particleSlider(
        "Twinkle",
        value: $particleSettings.twinkle,
        range: 0.0...1.5,
        formattedValue: "\(Int(particleSettings.twinkle * 100))%"
      )
      particleSlider(
        "Drift",
        value: $particleSettings.drift,
        range: 0.0...1.5,
        formattedValue: "\(Int(particleSettings.drift * 100))%"
      )

    }
  }

  private var multiColorAnimationSettings: some View {
    VStack(alignment: .leading, spacing: 12) {
      particleSlider(
        "Color speed",
        value: $particleSettings.multiColorAnimationSpeed,
        range: 0.1...4.0,
        formattedValue: String(format: "%.1fx", particleSettings.multiColorAnimationSpeed)
      )
      particleSlider(
        "Color smoothness",
        value: $particleSettings.multiColorAnimationSmoothness,
        range: 0.0...1.0,
        formattedValue: "\(Int(particleSettings.multiColorAnimationSmoothness * 100))%"
      )
      particleSlider(
        "Color wave",
        value: $particleSettings.multiColorAnimationSpread,
        range: 0.0...1.2,
        formattedValue: "\(Int(particleSettings.multiColorAnimationSpread * 100))%"
      )
    }
  }

  private var faceButtonSettings: some View {
    VStack(alignment: .leading, spacing: 12) {
      HStack {
        Toggle("Enable face buttons", isOn: $particleSettings.faceButtonsEnabled)
        settingResetButton("Add Face Buttons") {
          particleSettings.faceButtonsEnabled =
            defaults.faceButtonsEnabled
        }
      }

      particleSlider(
        "Face Buttons amount",
        value: $particleSettings.faceButtonAmount,
        range: 0.15...2.5,
        formattedValue: "\(Int(particleSettings.faceButtonAmount * 100))%"
      )
      particleSlider(
        "Face Buttons speed",
        value: $particleSettings.faceButtonSpeed,
        range: 0.05...1.25,
        formattedValue: String(format: "%.2fx", particleSettings.faceButtonSpeed)
      )
      particleSlider(
        "Face Buttons dispersion",
        value: $particleSettings.faceButtonDispersion,
        range: 0.25...2.2,
        formattedValue: "\(Int(particleSettings.faceButtonDispersion * 100))%"
      )
      particleSlider(
        "Face Buttons size",
        value: $particleSettings.faceButtonSize,
        range: 0.35...2.4,
        formattedValue: String(format: "%.1fx", particleSettings.faceButtonSize)
      )
      particleSlider(
        "Face Buttons opacity",
        value: $particleSettings.faceButtonOpacity,
        range: 0.1...1.4,
        formattedValue: "\(Int(particleSettings.faceButtonOpacity * 100))%"
      )
      particleSlider(
        "Face Buttons rotation",
        value: $particleSettings.faceButtonRotation,
        range: 0.0...2.0,
        formattedValue: "\(Int(particleSettings.faceButtonRotation * 100))%"
      )
      particleSlider(
        "Face Buttons pulse",
        value: $particleSettings.faceButtonPulse,
        range: 0.0...1.6,
        formattedValue: "\(Int(particleSettings.faceButtonPulse * 100))%"
      )
    }
  }

  private func dynamicSettingsGroup<Content: View>(
    _ title: String,
    isExpanded: Binding<Bool>,
    @ViewBuilder content: @escaping () -> Content
  ) -> some View {
    DisclosureGroup(isExpanded: isExpanded) {
      VStack(alignment: .leading, spacing: 12) {
        content()
      }
      .padding(.top, 10)
    } label: {
      Text(title)
        .font(.headline)
        .foregroundStyle(.white)
    }
    .dynamicSettingsCard()
  }

  private func particleSlider(
    _ title: String,
    value: Binding<Double>,
    range: ClosedRange<Double>,
    step: Double? = nil,
    formattedValue: String,
    resetValue: Double? = nil
  ) -> some View {
    DynamicSettingsValueSlider(
      title: title,
      value: value,
      range: range,
      step: step,
      formattedValue: formattedValue,
      resetValue: resetValue ?? particleResetValue(for: title)
    )
  }

  private func particleResetValue(for title: String) -> Double? {
    switch title {
    case "Amount": return defaults.amount
    case "Speed": return defaults.speed
    case "Direction of speed": return defaults.speedDirection
    case "Dispersion": return defaults.dispersion
    case "Y-axis spread": return defaults.verticalSpread
    case "Y-axis density": return defaults.verticalDensity
    case "Outer top/bottom dispersion": return defaults.outerDispersion
    case "Vertical level": return defaults.verticalLevel
    case "Size": return defaults.size
    case "Brightness": return defaults.brightness
    case "Particle opacity": return defaults.opacity
    case "Depth variation": return defaults.depthVariation
    case "Twinkle": return defaults.twinkle
    case "Drift": return defaults.drift
    case "See background preview before applying":
      return defaults.backgroundPreviewBeforeApplyingDuration
    case "See background preview after applying":
      return defaults.backgroundPreviewAfterApplyingDuration
    case "Motion speed": return defaults.backgrounds.multicolorAmbientSpeed
    case "FPS": return defaults.backgrounds.playStation2MenuFramesPerSecond
    case "Orbit angle": return defaults.backgrounds.playStation2MenuOrbitDegrees
    case "Yaw speed": return defaults.backgrounds.playStation2MenuYawSpeed
    case "Roll speed": return defaults.backgrounds.playStation2MenuRollSpeed
    case "Color speed": return defaults.multiColorAnimationSpeed
    case "Color smoothness": return defaults.multiColorAnimationSmoothness
    case "Color wave": return defaults.multiColorAnimationSpread
    case "Face Buttons amount": return defaults.faceButtonAmount
    case "Face Buttons speed": return defaults.faceButtonSpeed
    case "Face Buttons dispersion": return defaults.faceButtonDispersion
    case "Face Buttons size": return defaults.faceButtonSize
    case "Face Buttons opacity": return defaults.faceButtonOpacity
    case "Face Buttons rotation": return defaults.faceButtonRotation
    case "Face Buttons pulse": return defaults.faceButtonPulse
    default: return nil
    }
  }
}

// MARK: - PlayStation3SplinesSettingsControls

struct PlayStation3SplinesSettingsControls: View {
  @Binding var settings: PlayStation3SplinesSettings
  private let defaults = PlayStation3SplinesSettings()

  var body: some View {
    BackgroundSettingsResetHeader { settings = defaults }

    BackgroundControlSection("Visibility") {
      controlToggle(
        "Show splines", value: $settings.showsSplines, defaultValue: defaults.showsSplines)
      controlToggle(
        "Show native particles", value: $settings.showsParticles,
        defaultValue: defaults.showsParticles)
    }
    BackgroundControlSection("Spline geometry") {
      controlSlider(
        "Strand count", value: $settings.strandCount, range: 1...30,
        defaultValue: defaults.strandCount, format: count)
      controlSlider(
        "Vertical position", value: $settings.verticalPosition, range: 0.2...0.9,
        defaultValue: defaults.verticalPosition, format: percent)
      controlSlider(
        "Strand spacing", value: $settings.strandSpacing, range: 0...3,
        defaultValue: defaults.strandSpacing, format: multiplier)
      controlSlider(
        "Wave amplitude", value: $settings.waveAmplitude, range: 0...3,
        defaultValue: defaults.waveAmplitude, format: multiplier)
      controlSlider(
        "Detail amplitude", value: $settings.detailAmplitude, range: 0...3,
        defaultValue: defaults.detailAmplitude, format: multiplier)
      controlSlider(
        "Phase spread", value: $settings.phaseSpread, range: 0...3,
        defaultValue: defaults.phaseSpread, format: multiplier)
    }
    BackgroundControlSection("Spline material") {
      controlSlider(
        "Glow opacity", value: $settings.glowOpacity, range: 0...3,
        defaultValue: defaults.glowOpacity, format: percent)
      controlSlider(
        "Core opacity", value: $settings.coreOpacity, range: 0...3,
        defaultValue: defaults.coreOpacity, format: percent)
      controlSlider(
        "Glow width", value: $settings.glowWidth, range: 0.1...3, defaultValue: defaults.glowWidth,
        format: multiplier)
      controlSlider(
        "Core width", value: $settings.coreWidth, range: 0.1...4, defaultValue: defaults.coreWidth,
        format: multiplier)
      controlSlider(
        "Glow blur", value: $settings.glowBlur, range: 0...3, defaultValue: defaults.glowBlur,
        format: multiplier)
    }
    BackgroundControlSection("Native particles") {
      controlSlider(
        "Particle count", value: $settings.particleCount, range: 0...600,
        defaultValue: defaults.particleCount, format: count)
      controlSlider(
        "Particle speed", value: $settings.particleSpeed, range: 0...4,
        defaultValue: defaults.particleSpeed, format: multiplier)
      controlSlider(
        "Particle spread", value: $settings.particleSpread, range: 0...3,
        defaultValue: defaults.particleSpread, format: multiplier)
      controlSlider(
        "Particle size", value: $settings.particleSize, range: 0.1...5,
        defaultValue: defaults.particleSize, format: multiplier)
      controlSlider(
        "Particle opacity", value: $settings.particleOpacity, range: 0...3,
        defaultValue: defaults.particleOpacity, format: percent)
      controlSlider(
        "Twinkle speed", value: $settings.particleTwinkle, range: 0...4,
        defaultValue: defaults.particleTwinkle, format: multiplier)
    }
    BackgroundControlSection("Environment") {
      controlSlider(
        "Background intensity", value: $settings.backgroundIntensity, range: 0...2,
        defaultValue: defaults.backgroundIntensity, format: percent)
      controlSlider(
        "Vignette intensity", value: $settings.vignetteIntensity, range: 0...3,
        defaultValue: defaults.vignetteIntensity, format: percent)
    }
  }
}

// MARK: - PlayStation4ParticlesSettingsControls

struct PlayStation4ParticlesSettingsControls: View {
  @Binding var settings: PlayStation4ParticlesSettings
  private let defaults = PlayStation4ParticlesSettings()

  var body: some View {
    BackgroundSettingsResetHeader { settings = defaults }

    BackgroundControlSection("Visibility") {
      controlToggle(
        "Show native particles", value: $settings.showsParticles,
        defaultValue: defaults.showsParticles)
      controlToggle(
        "Show wave bundle", value: $settings.showsWaves, defaultValue: defaults.showsWaves)
      controlToggle(
        "Show wave glow", value: $settings.showsWaveGlow, defaultValue: defaults.showsWaveGlow)
      controlToggle(
        "Show wave cores", value: $settings.showsWaveCores, defaultValue: defaults.showsWaveCores)
    }
    BackgroundControlSection("Native particles") {
      controlSlider(
        "Particle count", value: $settings.particleCount, range: 0...700,
        defaultValue: defaults.particleCount, format: count)
      controlSlider(
        "Particle speed", value: $settings.particleSpeed, range: 0...4,
        defaultValue: defaults.particleSpeed, format: multiplier)
      controlSlider(
        "Particle height", value: $settings.particleVerticalPosition, range: 0...1,
        defaultValue: defaults.particleVerticalPosition, format: percent)
      controlSlider(
        "Particle spread", value: $settings.particleSpread, range: 0...3,
        defaultValue: defaults.particleSpread, format: multiplier)
      controlSlider(
        "Particle size", value: $settings.particleSize, range: 0.1...5,
        defaultValue: defaults.particleSize, format: multiplier)
      controlSlider(
        "Particle opacity", value: $settings.particleOpacity, range: 0...3,
        defaultValue: defaults.particleOpacity, format: percent)
      controlSlider(
        "Twinkle speed", value: $settings.particleTwinkle, range: 0...4,
        defaultValue: defaults.particleTwinkle, format: multiplier)
    }
    BackgroundControlSection("Wave geometry") {
      controlSlider(
        "Wave count", value: $settings.waveCount, range: 1...16, defaultValue: defaults.waveCount,
        format: count)
      controlSlider(
        "Wave height", value: $settings.waveVerticalPosition, range: 0...1,
        defaultValue: defaults.waveVerticalPosition, format: percent)
      controlSlider(
        "Wave spacing", value: $settings.waveSpacing, range: 0...3,
        defaultValue: defaults.waveSpacing, format: multiplier)
      controlSlider(
        "Wave amplitude", value: $settings.waveAmplitude, range: 0...3,
        defaultValue: defaults.waveAmplitude, format: multiplier)
      controlSlider(
        "Wave curvature", value: $settings.waveCurvature, range: 0...3,
        defaultValue: defaults.waveCurvature, format: multiplier)
      controlSlider(
        "Phase spread", value: $settings.phaseSpread, range: 0...3,
        defaultValue: defaults.phaseSpread, format: multiplier)
    }
    BackgroundControlSection("Wave material") {
      controlSlider(
        "Glow blur", value: $settings.glowBlur, range: 0...50, defaultValue: defaults.glowBlur,
        format: points)
      controlSlider(
        "Glow width", value: $settings.glowWidth, range: 0.1...3, defaultValue: defaults.glowWidth,
        format: multiplier)
      controlSlider(
        "Glow opacity", value: $settings.glowOpacity, range: 0...3,
        defaultValue: defaults.glowOpacity, format: percent)
      controlSlider(
        "Core width", value: $settings.coreWidth, range: 0.1...4, defaultValue: defaults.coreWidth,
        format: multiplier)
      controlSlider(
        "Core opacity", value: $settings.coreOpacity, range: 0...3,
        defaultValue: defaults.coreOpacity, format: percent)
    }
    BackgroundControlSection("Environment") {
      controlSlider(
        "Background intensity", value: $settings.backgroundIntensity, range: 0...2,
        defaultValue: defaults.backgroundIntensity, format: percent)
      controlSlider(
        "Vignette intensity", value: $settings.vignetteIntensity, range: 0...3,
        defaultValue: defaults.vignetteIntensity, format: percent)
    }
  }
}

// MARK: - PlayStation4WavesSettingsControls

struct PlayStation4WavesSettingsControls: View {
  @Binding var settings: PlayStation4WavesSettings
  private let defaults = PlayStation4WavesSettings()

  var body: some View {
    BackgroundSettingsResetHeader { settings = defaults }

    BackgroundControlSection("Visibility") {
      controlToggle(
        "Show ambient glows", value: $settings.showsAmbientGlows,
        defaultValue: defaults.showsAmbientGlows)
      controlToggle(
        "Show native particles", value: $settings.showsParticles,
        defaultValue: defaults.showsParticles)
      controlToggle("Show waves", value: $settings.showsWaves, defaultValue: defaults.showsWaves)
      controlToggle(
        "Show wave glow", value: $settings.showsWaveGlow, defaultValue: defaults.showsWaveGlow)
      controlToggle(
        "Show wave cores", value: $settings.showsWaveCores, defaultValue: defaults.showsWaveCores)
    }
    BackgroundControlSection("Native particles") {
      controlSlider(
        "Particle count", value: $settings.particleCount, range: 0...300,
        defaultValue: defaults.particleCount, format: count)
      controlSlider(
        "Particle speed", value: $settings.particleSpeed, range: 0...4,
        defaultValue: defaults.particleSpeed, format: multiplier)
      controlSlider(
        "Particle spread", value: $settings.particleSpread, range: 0...3,
        defaultValue: defaults.particleSpread, format: multiplier)
      controlSlider(
        "Particle size", value: $settings.particleSize, range: 0.1...5,
        defaultValue: defaults.particleSize, format: multiplier)
      controlSlider(
        "Particle opacity", value: $settings.particleOpacity, range: 0...3,
        defaultValue: defaults.particleOpacity, format: percent)
    }
    BackgroundControlSection("Wave geometry") {
      controlSlider(
        "Wave count", value: $settings.waveCount, range: 1...16, defaultValue: defaults.waveCount,
        format: count)
      controlSlider(
        "Wave height", value: $settings.waveVerticalPosition, range: 0...1,
        defaultValue: defaults.waveVerticalPosition, format: percent)
      controlSlider(
        "Wave spacing", value: $settings.waveSpacing, range: 0...3,
        defaultValue: defaults.waveSpacing, format: multiplier)
      controlSlider(
        "Wave amplitude", value: $settings.waveAmplitude, range: 0...3,
        defaultValue: defaults.waveAmplitude, format: multiplier)
      controlSlider(
        "Wave curvature", value: $settings.waveCurvature, range: 0...3,
        defaultValue: defaults.waveCurvature, format: multiplier)
      controlSlider(
        "Phase spread", value: $settings.phaseSpread, range: 0...3,
        defaultValue: defaults.phaseSpread, format: multiplier)
    }
    BackgroundControlSection("Wave material") {
      controlSlider(
        "Glow blur", value: $settings.glowBlur, range: 0...50, defaultValue: defaults.glowBlur,
        format: points)
      controlSlider(
        "Glow width", value: $settings.glowWidth, range: 0.1...3, defaultValue: defaults.glowWidth,
        format: multiplier)
      controlSlider(
        "Glow opacity", value: $settings.glowOpacity, range: 0...3,
        defaultValue: defaults.glowOpacity, format: percent)
      controlSlider(
        "Core width", value: $settings.coreWidth, range: 0.1...4, defaultValue: defaults.coreWidth,
        format: multiplier)
      controlSlider(
        "Core opacity", value: $settings.coreOpacity, range: 0...3,
        defaultValue: defaults.coreOpacity, format: percent)
    }
    BackgroundControlSection("Environment") {
      controlSlider(
        "Ambient glow scale", value: $settings.ambientGlowScale, range: 0.1...3,
        defaultValue: defaults.ambientGlowScale, format: multiplier)
      controlSlider(
        "Ambient glow intensity", value: $settings.ambientGlowIntensity, range: 0...3,
        defaultValue: defaults.ambientGlowIntensity, format: percent)
      controlSlider(
        "Vignette intensity", value: $settings.vignetteIntensity, range: 0...3,
        defaultValue: defaults.vignetteIntensity, format: percent)
    }
  }
}

// MARK: - PlayStationRibbonsSettingsControls

struct PlayStationRibbonsSettingsControls: View {
  @Binding var settings: PlayStationRibbonsSettings
  private let defaults = PlayStationRibbonsSettings()

  var body: some View {
    BackgroundSettingsResetHeader { settings = defaults }

    BackgroundControlSection("Visibility") {
      controlToggle("Show panels", value: $settings.showsPanels, defaultValue: defaults.showsPanels)
      controlToggle(
        "Show panel glow", value: $settings.showsPanelGlow, defaultValue: defaults.showsPanelGlow)
      controlToggle(
        "Show panel edges", value: $settings.showsPanelEdges, defaultValue: defaults.showsPanelEdges
      )
      controlToggle(
        "Show native particles", value: $settings.showsParticles,
        defaultValue: defaults.showsParticles)
    }
    BackgroundControlSection("Panel geometry") {
      controlSlider(
        "Panel count", value: $settings.panelCount, range: 1...12,
        defaultValue: defaults.panelCount, format: count)
      controlSlider(
        "Panel height", value: $settings.panelVerticalPosition, range: -0.2...0.8,
        defaultValue: defaults.panelVerticalPosition, format: percent)
      controlSlider(
        "Panel spacing", value: $settings.panelSpacing, range: 0...2,
        defaultValue: defaults.panelSpacing, format: multiplier)
      controlSlider(
        "Panel amplitude", value: $settings.panelAmplitude, range: 0...3,
        defaultValue: defaults.panelAmplitude, format: multiplier)
      controlSlider(
        "Panel thickness", value: $settings.panelThickness, range: 0.1...4,
        defaultValue: defaults.panelThickness, format: multiplier)
      controlSlider(
        "Panel curvature", value: $settings.panelCurvature, range: 0...3,
        defaultValue: defaults.panelCurvature, format: multiplier)
      controlSlider(
        "Phase spread", value: $settings.phaseSpread, range: 0...3,
        defaultValue: defaults.phaseSpread, format: multiplier)
    }
    BackgroundControlSection("Panel material") {
      controlSlider(
        "Fill opacity", value: $settings.panelFillOpacity, range: 0...3,
        defaultValue: defaults.panelFillOpacity, format: percent)
      controlSlider(
        "Glow opacity", value: $settings.panelGlowOpacity, range: 0...3,
        defaultValue: defaults.panelGlowOpacity, format: percent)
      controlSlider(
        "Glow blur", value: $settings.panelGlowBlur, range: 0...3,
        defaultValue: defaults.panelGlowBlur, format: multiplier)
      controlSlider(
        "Edge opacity", value: $settings.panelEdgeOpacity, range: 0...3,
        defaultValue: defaults.panelEdgeOpacity, format: percent)
      controlSlider(
        "Edge width", value: $settings.panelEdgeWidth, range: 0.1...4,
        defaultValue: defaults.panelEdgeWidth, format: multiplier)
    }
    BackgroundControlSection("Native particles") {
      controlSlider(
        "Particle count", value: $settings.particleCount, range: 0...500,
        defaultValue: defaults.particleCount, format: count)
      controlSlider(
        "Particle speed", value: $settings.particleSpeed, range: 0...4,
        defaultValue: defaults.particleSpeed, format: multiplier)
      controlSlider(
        "Particle drift", value: $settings.particleDrift, range: 0...4,
        defaultValue: defaults.particleDrift, format: multiplier)
      controlSlider(
        "Vertical spread", value: $settings.particleVerticalSpread, range: 0...3,
        defaultValue: defaults.particleVerticalSpread, format: multiplier)
      controlSlider(
        "Particle size", value: $settings.particleSize, range: 0.1...5,
        defaultValue: defaults.particleSize, format: multiplier)
      controlSlider(
        "Particle opacity", value: $settings.particleOpacity, range: 0...3,
        defaultValue: defaults.particleOpacity, format: percent)
    }
    BackgroundControlSection("Environment") {
      controlSlider(
        "Ambient glow scale", value: $settings.ambientGlowScale, range: 0.1...3,
        defaultValue: defaults.ambientGlowScale, format: multiplier)
      controlSlider(
        "Ambient glow intensity", value: $settings.ambientGlowIntensity, range: 0...3,
        defaultValue: defaults.ambientGlowIntensity, format: percent)
      controlSlider(
        "Vignette intensity", value: $settings.vignetteIntensity, range: 0...3,
        defaultValue: defaults.vignetteIntensity, format: percent)
    }
  }
}

// MARK: - PlayStationPortableBlurSettingsControls

struct PlayStationPortableBlurSettingsControls: View {
  @Binding var settings: PlayStationPortableBlurSettings
  private let defaults = PlayStationPortableBlurSettings()

  var body: some View {
    BackgroundSettingsResetHeader { settings = defaults }

    BackgroundControlSection("Visibility") {
      controlToggle(
        "Show ribbons", value: $settings.showsRibbons, defaultValue: defaults.showsRibbons)
      controlToggle(
        "Show ribbon glow", value: $settings.showsGlow, defaultValue: defaults.showsGlow)
      controlToggle(
        "Show ribbon cores", value: $settings.showsCores, defaultValue: defaults.showsCores)
      controlToggle(
        "Alternate direction", value: $settings.alternatesDirection,
        defaultValue: defaults.alternatesDirection)
    }
    BackgroundControlSection("Ribbon geometry") {
      controlSlider(
        "Ribbon count", value: $settings.ribbonCount, range: 1...14,
        defaultValue: defaults.ribbonCount, format: count)
      controlSlider(
        "Vertical offset", value: $settings.verticalOffset, range: -0.6...0.6,
        defaultValue: defaults.verticalOffset, format: signedPercent)
      controlSlider(
        "Wave amplitude", value: $settings.waveAmplitude, range: 0...3,
        defaultValue: defaults.waveAmplitude, format: multiplier)
      controlSlider(
        "Detail amplitude", value: $settings.detailAmplitude, range: 0...3,
        defaultValue: defaults.detailAmplitude, format: multiplier)
      controlSlider(
        "Phase spread", value: $settings.phaseSpread, range: 0...3,
        defaultValue: defaults.phaseSpread, format: multiplier)
    }
    BackgroundControlSection("Ribbon material") {
      controlSlider(
        "Broad width", value: $settings.broadWidth, range: 0.1...4,
        defaultValue: defaults.broadWidth, format: multiplier)
      controlSlider(
        "Glow blur", value: $settings.glowBlur, range: 0...3, defaultValue: defaults.glowBlur,
        format: multiplier)
      controlSlider(
        "Glow opacity", value: $settings.glowOpacity, range: 0...3,
        defaultValue: defaults.glowOpacity, format: percent)
      controlSlider(
        "Core width", value: $settings.coreWidth, range: 0.1...4, defaultValue: defaults.coreWidth,
        format: multiplier)
      controlSlider(
        "Core blur", value: $settings.coreBlur, range: 0...3, defaultValue: defaults.coreBlur,
        format: multiplier)
      controlSlider(
        "Core opacity", value: $settings.coreOpacity, range: 0...3,
        defaultValue: defaults.coreOpacity, format: percent)
    }
    BackgroundControlSection("Environment") {
      controlSlider(
        "Background intensity", value: $settings.backgroundIntensity, range: 0...2,
        defaultValue: defaults.backgroundIntensity, format: percent)
      controlSlider(
        "Ambient glow scale", value: $settings.ambientGlowScale, range: 0.1...3,
        defaultValue: defaults.ambientGlowScale, format: multiplier)
      controlSlider(
        "Ambient glow intensity", value: $settings.ambientGlowIntensity, range: 0...3,
        defaultValue: defaults.ambientGlowIntensity, format: percent)
      controlSlider(
        "Vignette intensity", value: $settings.vignetteIntensity, range: 0...3,
        defaultValue: defaults.vignetteIntensity, format: percent)
    }
  }
}

// MARK: - PlayStation3XMBMartSettingsControls

struct PlayStation3XMBMartSettingsControls: View {
  @Binding var settings: PlayStation3XMBSettings
  @Binding var isPresetExplicit: Bool
  @AppStorage("ARMSX2iOSDynamicExpandedNestedGroups")
  private var expandedGroups = ""
  private let defaults = PlayStation3XMBSettings()

  var body: some View {
    VStack(alignment: .leading, spacing: 12) {
      HStack {
        Spacer()

        Button {
          settings = defaults
          isPresetExplicit = false
        } label: {
          Label("Reset", systemImage: "arrow.counterclockwise")
            .font(.caption.weight(.semibold))
        }
        .buttonStyle(.borderless)
      }

      xmbMartSettingGroup("Background") {
        HStack {
          Picker(
            "Gradient preset",
            selection: gradientPresetBinding
          ) {
            ForEach(PlayStation3XMBGradientPreset.allCases) { preset in
              Text(preset.title).tag(preset)
            }
          }
          .pickerStyle(.menu)
          settingResetButton("Gradient preset") {
            settings.gradientPreset =
              defaults.gradientPreset
            isPresetExplicit = false
          }
        }

        particleSlider(
          "Color red",
          value: $settings.colorR,
          range: 0...255,
          formattedValue: String(format: "%.0f", settings.colorR)
        )
        particleSlider(
          "Color green",
          value: $settings.colorG,
          range: 0...255,
          formattedValue: String(format: "%.0f", settings.colorG)
        )
        particleSlider(
          "Color blue",
          value: $settings.colorB,
          range: 0...255,
          formattedValue: String(format: "%.0f", settings.colorB)
        )
        particleSlider(
          "Gradient top multiplier",
          value: $settings.gradientTopMul,
          range: 0...0.3,
          formattedValue: String(format: "%.3f", settings.gradientTopMul)
        )
        particleSlider(
          "Gradient bottom multiplier",
          value: $settings.gradientBotMul,
          range: 0.2...1.2,
          formattedValue: String(format: "%.3f", settings.gradientBotMul)
        )
      }

      xmbMartSettingGroup("Particles") {
        particleSlider(
          "Count",
          value: $settings.particleCount,
          range: 10...4000,
          formattedValue: String(format: "%.0f", settings.particleCount)
        )
        particleSlider(
          "Opacity",
          value: $settings.particleOpacity,
          range: 0...1,
          formattedValue: String(format: "%.2f", settings.particleOpacity)
        )
        particleSlider(
          "Base size",
          value: $settings.particleSizeBase,
          range: 1...40,
          formattedValue: String(format: "%.1f", settings.particleSizeBase)
        )
        particleSlider(
          "Size variance",
          value: $settings.particleSizeVariance,
          range: 0...50,
          formattedValue: String(format: "%.1f", settings.particleSizeVariance)
        )
        particleSlider(
          "Particle flow speed",
          value: $settings.particleFlowSpeed,
          range: 0...3,
          formattedValue: String(format: "%.2f", settings.particleFlowSpeed)
        )
        particleSlider(
          "Y-axis spread",
          value: $settings.particleVerticalSpread,
          range: 0.25...3.0,
          formattedValue: "\(Int(settings.particleVerticalSpread * 100))%",
          resetValue: defaults.particleVerticalSpread
        )
        particleSlider(
          "Y-axis density",
          value: $settings.particleVerticalDensity,
          range: 0.25...3.0,
          formattedValue: "\(Int(settings.particleVerticalDensity * 100))%",
          resetValue: defaults.particleVerticalDensity
        )
        particleSlider(
          "Outer top/bottom dispersion",
          value: $settings.particleOuterDispersion,
          range: 0.0...2.5,
          formattedValue: "\(Int(settings.particleOuterDispersion * 100))%",
          resetValue: defaults.particleOuterDispersion
        )
        particleSlider(
          "Depth variation",
          value: $settings.particleDepthVariation,
          range: 0.0...1.5,
          formattedValue: "\(Int(settings.particleDepthVariation * 100))%",
          resetValue: defaults.particleDepthVariation
        )
        HStack {
          Toggle(
            "Follow wave Y-axis",
            isOn: $settings.particlesFollowWaveY
          )
          settingResetButton("Follow wave Y-axis") {
            settings.particlesFollowWaveY =
              defaults.particlesFollowWaveY
          }
        }
      }

      xmbMartSettingGroup("Spline motion") {
        particleSlider(
          "Flow speed",
          value: $settings.flowSpeed,
          range: 0...1.2,
          formattedValue: String(format: "%.3f", settings.flowSpeed)
        )
        particleSlider(
          "Tension",
          value: $settings.tension,
          range: 0...0.5,
          formattedValue: String(format: "%.3f", settings.tension)
        )
        particleSlider(
          "Damping",
          value: $settings.damping,
          range: 0...0.002,
          formattedValue: String(format: "%.5f", settings.damping)
        )
        particleSlider(
          "Length",
          value: $settings.length,
          range: 0.05...1.2,
          formattedValue: String(format: "%.3f", settings.length)
        )
        particleSlider(
          "Spacing",
          value: $settings.spacing,
          range: 10...800,
          formattedValue: String(format: "%.0f", settings.spacing)
        )
        particleSlider(
          "Time step",
          value: $settings.timeStep,
          range: 0.1...4,
          formattedValue: String(format: "%.2f", settings.timeStep)
        )
      }

      xmbMartSettingGroup("Bands and travel") {
        particleSlider(
          "Band amplitude",
          value: $settings.bandAmplitude,
          range: 0...0.6,
          formattedValue: String(format: "%.3f", settings.bandAmplitude)
        )
        particleSlider(
          "Secondary frequency",
          value: $settings.bandSecondaryFrequency,
          range: 0.5...16,
          formattedValue: String(format: "%.1f", settings.bandSecondaryFrequency)
        )
        particleSlider(
          "Secondary amplitude",
          value: $settings.bandSecondaryAmplitude,
          range: 0...0.12,
          formattedValue: String(format: "%.3f", settings.bandSecondaryAmplitude)
        )
        particleSlider(
          "Travel speed 1",
          value: $settings.travelSpeed1,
          range: 0...1.5,
          formattedValue: String(format: "%.2f", settings.travelSpeed1)
        )
        particleSlider(
          "Travel amplitude 1",
          value: $settings.travelAmplitude1,
          range: 0...0.08,
          formattedValue: String(format: "%.3f", settings.travelAmplitude1)
        )
        particleSlider(
          "Travel speed 2",
          value: $settings.travelSpeed2,
          range: 0...1.5,
          formattedValue: String(format: "%.2f", settings.travelSpeed2)
        )
        particleSlider(
          "Travel amplitude 2",
          value: $settings.travelAmplitude2,
          range: 0...0.08,
          formattedValue: String(format: "%.3f", settings.travelAmplitude2)
        )
      }

      xmbMartSettingGroup("Wave shaping") {
        particleSlider(
          "Perturbation",
          value: $settings.perturbation,
          range: 0...0.3,
          formattedValue: String(format: "%.3f", settings.perturbation)
        )
        particleSlider(
          "Perturbation scale",
          value: $settings.perturbationScale,
          range: 0...0.3,
          formattedValue: String(format: "%.3f", settings.perturbationScale)
        )
        particleSlider(
          "Cosine amplitude",
          value: $settings.waveCosineAmplitude,
          range: 0...0.3,
          formattedValue: String(format: "%.3f", settings.waveCosineAmplitude)
        )
        particleSlider(
          "Wave bias",
          value: $settings.waveBias,
          range: -0.3...0.3,
          formattedValue: String(format: "%.3f", settings.waveBias)
        )
        particleSlider(
          "Height scale",
          value: $settings.waveHeightScale,
          range: 0...1,
          formattedValue: String(format: "%.3f", settings.waveHeightScale)
        )
        particleSlider(
          "Soft clip",
          value: $settings.waveSoftClip,
          range: 0.05...0.5,
          formattedValue: String(format: "%.3f", settings.waveSoftClip)
        )
      }

      xmbMartSettingGroup("Reverse pipeline") {
        particleSlider(
          "Pipeline blend",
          value: $settings.reversePipelineBlend,
          range: 0...1,
          formattedValue: String(format: "%.2f", settings.reversePipelineBlend)
        )
        particleSlider(
          "Descriptor strength",
          value: $settings.reverseDescriptorStrength,
          range: 0...2,
          formattedValue: String(format: "%.2f", settings.reverseDescriptorStrength)
        )
        particleSlider(
          "Descriptor seed",
          value: $settings.reverseSyntheticDescriptorSeed,
          range: 0...100_000,
          formattedValue: String(format: "%.0f", settings.reverseSyntheticDescriptorSeed)
        )
        particleSlider(
          "Descriptor motion",
          value: $settings.reverseSyntheticDescriptorMotion,
          range: 0...5,
          formattedValue: String(format: "%.2f", settings.reverseSyntheticDescriptorMotion)
        )
        particleSlider(
          "Kernel gain",
          value: $settings.reverseKernelGain,
          range: 0...1,
          formattedValue: String(format: "%.3f", settings.reverseKernelGain)
        )
        particleSlider(
          "Normalize gain",
          value: $settings.reverseNormalizeGain,
          range: 0...2,
          formattedValue: String(format: "%.2f", settings.reverseNormalizeGain)
        )
        particleSlider(
          "Kernel phase step",
          value: $settings.reverseKernelPhaseStep,
          range: 0...8,
          formattedValue: String(format: "%.2f", settings.reverseKernelPhaseStep)
        )
        particleSlider(
          "Index jitter",
          value: $settings.reverseIndexJitter,
          range: 0...0.5,
          formattedValue: String(format: "%.3f", settings.reverseIndexJitter)
        )
        particleSlider(
          "Temporal smoothing",
          value: $settings.reverseTemporalSmooth,
          range: 0...0.98,
          formattedValue: String(format: "%.2f", settings.reverseTemporalSmooth)
        )
      }

      xmbMartSettingGroup("Material") {
        particleSlider(
          "Fresnel power",
          value: $settings.fresnelPower,
          range: 0.2...8,
          formattedValue: String(format: "%.2f", settings.fresnelPower)
        )
        particleSlider(
          "Fresnel scale",
          value: $settings.fresnelScale,
          range: 0...2,
          formattedValue: String(format: "%.2f", settings.fresnelScale)
        )
        particleSlider(
          "Wave opacity",
          value: $settings.opacity,
          range: 0...1,
          formattedValue: String(format: "%.3f", settings.opacity)
        )
        particleSlider(
          "Spline brightness",
          value: $settings.brightness,
          range: 0...2,
          formattedValue: String(format: "%.2f", settings.brightness)
        )
        particleSlider(
          "Z detail scale",
          value: $settings.zDetailScale,
          range: 0...0.25,
          formattedValue: String(format: "%.3f", settings.zDetailScale)
        )
      }

      xmbMartSettingGroup("Free-form deformation") {
        particleSlider(
          "Scale 1 X",
          value: $settings.ffdScale1X,
          range: 0...8,
          formattedValue: String(format: "%.2f", settings.ffdScale1X)
        )
        particleSlider(
          "Scale 1 Y",
          value: $settings.ffdScale1Y,
          range: 0...3,
          formattedValue: String(format: "%.2f", settings.ffdScale1Y)
        )
        particleSlider(
          "Scale 1 Z",
          value: $settings.ffdScale1Z,
          range: 0...3,
          formattedValue: String(format: "%.2f", settings.ffdScale1Z)
        )
        particleSlider(
          "Scale 2 X",
          value: $settings.ffdScale2X,
          range: 0...8,
          formattedValue: String(format: "%.2f", settings.ffdScale2X)
        )
        particleSlider(
          "Scale 2 Y",
          value: $settings.ffdScale2Y,
          range: 0...3,
          formattedValue: String(format: "%.2f", settings.ffdScale2Y)
        )
        particleSlider(
          "Scale 2 Z",
          value: $settings.ffdScale2Z,
          range: 0...6,
          formattedValue: String(format: "%.2f", settings.ffdScale2Z)
        )
        particleSlider(
          "Offset X",
          value: $settings.ffdOffsetX,
          range: -2...2,
          formattedValue: String(format: "%.2f", settings.ffdOffsetX)
        )
        particleSlider(
          "Offset Y",
          value: $settings.ffdOffsetY,
          range: -2...2,
          formattedValue: String(format: "%.2f", settings.ffdOffsetY)
        )
        particleSlider(
          "Offset Z",
          value: $settings.ffdOffsetZ,
          range: -2...2,
          formattedValue: String(format: "%.2f", settings.ffdOffsetZ)
        )
        particleSlider(
          "Y amplitude",
          value: $settings.ffdYAmplitude,
          range: 0...0.3,
          formattedValue: String(format: "%.3f", settings.ffdYAmplitude)
        )
        particleSlider(
          "Z amplitude",
          value: $settings.ffdZAmplitude,
          range: 0...0.3,
          formattedValue: String(format: "%.3f", settings.ffdZAmplitude)
        )
      }
    }
  }

  private var gradientPresetBinding: Binding<PlayStation3XMBGradientPreset> {
    Binding(
      get: { settings.gradientPreset },
      set: { preset in
        settings.gradientPreset = preset
        isPresetExplicit = true
      }
    )
  }

  private func particleSlider(
    _ title: String,
    value: Binding<Double>,
    range: ClosedRange<Double>,
    step: Double? = nil,
    formattedValue: String,
    resetValue: Double? = nil
  ) -> some View {
    DynamicSettingsValueSlider(
      title: title,
      value: value,
      range: range,
      step: step,
      formattedValue: formattedValue,
      resetValue: resetValue ?? defaultValue(for: title)
    )
  }

  private func xmbMartSettingGroup<Content: View>(
    _ title: String,
    @ViewBuilder content: @escaping () -> Content
  ) -> some View {
    DisclosureGroup(
      isExpanded: DynamicSettingsExpansion.binding(
        storage: $expandedGroups,
        identifier: "mart.\(title)"
      )
    ) {
      VStack(alignment: .leading, spacing: 12) {
        content()
      }
      .padding(.top, 10)
    } label: {
      Text(title)
        .font(.subheadline.weight(.semibold))
    }
  }

  private func defaultValue(for title: String) -> Double? {
    switch title {
    case "Color red": return defaults.colorR
    case "Color green": return defaults.colorG
    case "Color blue": return defaults.colorB
    case "Gradient top multiplier": return defaults.gradientTopMul
    case "Gradient bottom multiplier": return defaults.gradientBotMul
    case "Count": return defaults.particleCount
    case "Opacity": return defaults.particleOpacity
    case "Base size": return defaults.particleSizeBase
    case "Size variance": return defaults.particleSizeVariance
    case "Particle flow speed": return defaults.particleFlowSpeed
    case "Y-axis spread": return defaults.particleVerticalSpread
    case "Y-axis density": return defaults.particleVerticalDensity
    case "Outer top/bottom dispersion": return defaults.particleOuterDispersion
    case "Depth variation": return defaults.particleDepthVariation
    case "Flow speed": return defaults.flowSpeed
    case "Tension": return defaults.tension
    case "Damping": return defaults.damping
    case "Length": return defaults.length
    case "Spacing": return defaults.spacing
    case "Time step": return defaults.timeStep
    case "Band amplitude": return defaults.bandAmplitude
    case "Secondary frequency": return defaults.bandSecondaryFrequency
    case "Secondary amplitude": return defaults.bandSecondaryAmplitude
    case "Travel speed 1": return defaults.travelSpeed1
    case "Travel amplitude 1": return defaults.travelAmplitude1
    case "Travel speed 2": return defaults.travelSpeed2
    case "Travel amplitude 2": return defaults.travelAmplitude2
    case "Perturbation": return defaults.perturbation
    case "Perturbation scale": return defaults.perturbationScale
    case "Cosine amplitude": return defaults.waveCosineAmplitude
    case "Wave bias": return defaults.waveBias
    case "Height scale": return defaults.waveHeightScale
    case "Soft clip": return defaults.waveSoftClip
    case "Pipeline blend": return defaults.reversePipelineBlend
    case "Descriptor strength": return defaults.reverseDescriptorStrength
    case "Descriptor seed": return defaults.reverseSyntheticDescriptorSeed
    case "Descriptor motion": return defaults.reverseSyntheticDescriptorMotion
    case "Kernel gain": return defaults.reverseKernelGain
    case "Normalize gain": return defaults.reverseNormalizeGain
    case "Kernel phase step": return defaults.reverseKernelPhaseStep
    case "Index jitter": return defaults.reverseIndexJitter
    case "Temporal smoothing": return defaults.reverseTemporalSmooth
    case "Fresnel power": return defaults.fresnelPower
    case "Fresnel scale": return defaults.fresnelScale
    case "Wave opacity": return defaults.opacity
    case "Spline brightness": return defaults.brightness
    case "Z detail scale": return defaults.zDetailScale
    case "Scale 1 X": return defaults.ffdScale1X
    case "Scale 1 Y": return defaults.ffdScale1Y
    case "Scale 1 Z": return defaults.ffdScale1Z
    case "Scale 2 X": return defaults.ffdScale2X
    case "Scale 2 Y": return defaults.ffdScale2Y
    case "Scale 2 Z": return defaults.ffdScale2Z
    case "Offset X": return defaults.ffdOffsetX
    case "Offset Y": return defaults.ffdOffsetY
    case "Offset Z": return defaults.ffdOffsetZ
    case "Y amplitude": return defaults.ffdYAmplitude
    case "Z amplitude": return defaults.ffdZAmplitude
    default: return nil
    }
  }
}
