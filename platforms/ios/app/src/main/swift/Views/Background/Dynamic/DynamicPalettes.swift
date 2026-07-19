// DynamicPalettes.swift — Dynamic background palette definitions
// SPDX-License-Identifier: GPL-3.0+

import Foundation
import SwiftUI
import UIKit

// MARK: - ThemePalette

enum ThemePalette: String, CaseIterable, Identifiable, Equatable, Codable {
  case blue
  case violet
  case cyan
  case pink
  case gold
  case crimson
  case emerald
  case midnight
  case burgundy
  case graphite
  case noir
  case silver
  case slate
  case charcoal
  case navy
  case deepTeal
  case lavender
  case sunset
  case aurora
  case multicolor
  case xmbFrostedPearl
  case xmbLunarGraphite
  case xmbGildedSun
  case xmbAntiqueDusk
  case xmbSpringMeadow
  case xmbSageMidnight
  case xmbSakuraBloom
  case xmbRoseTwilight
  case xmbCloverField
  case xmbForestVelvet
  case xmbOrchidHaze
  case xmbVioletNocturne
  case xmbTurquoiseLagoon
  case xmbTidalAbyss
  case xmbAzureHorizon
  case xmbOceanMidnight
  case xmbAmethystGlow
  case xmbPurpleEclipse
  case xmbHarvestGold
  case xmbEmberNight
  case xmbBronzeAutumn
  case xmbHearthShadow
  case xmbWinterCoral
  case xmbCinderRed

  var id: String { rawValue }

  static let primaryPalettes: [ThemePalette] = [
    .blue, .violet, .cyan, .pink, .gold, .crimson, .emerald, .midnight,
    .burgundy, .graphite, .noir, .silver, .slate, .charcoal, .navy,
    .deepTeal, .lavender, .sunset, .aurora, .multicolor,
  ]

  static let morePalettes: [ThemePalette] = [
    .xmbFrostedPearl, .xmbLunarGraphite,
    .xmbGildedSun, .xmbAntiqueDusk,
    .xmbSpringMeadow, .xmbSageMidnight,
    .xmbSakuraBloom, .xmbRoseTwilight,
    .xmbCloverField, .xmbForestVelvet,
    .xmbOrchidHaze, .xmbVioletNocturne,
    .xmbTurquoiseLagoon, .xmbTidalAbyss,
    .xmbAzureHorizon, .xmbOceanMidnight,
    .xmbAmethystGlow, .xmbPurpleEclipse,
    .xmbHarvestGold, .xmbEmberNight,
    .xmbBronzeAutumn, .xmbHearthShadow,
    .xmbWinterCoral, .xmbCinderRed,
  ]

  var isMorePalette: Bool {
    Self.morePalettes.contains(self)
  }

  var lightRibbonPaletteForMart: ThemePalette {
    switch self {
    case .blue, .violet, .cyan, .pink, .gold, .crimson, .emerald,
      .silver, .lavender, .sunset, .aurora, .multicolor:
      return self
    case .midnight: return .blue
    case .burgundy: return .pink
    case .graphite, .noir: return .silver
    case .slate: return .lavender
    case .charcoal: return .gold
    case .navy: return .cyan
    case .deepTeal: return .emerald
    case .xmbFrostedPearl: return self
    case .xmbLunarGraphite: return .xmbFrostedPearl
    case .xmbGildedSun: return self
    case .xmbAntiqueDusk: return .xmbGildedSun
    case .xmbSpringMeadow: return self
    case .xmbSageMidnight: return .xmbSpringMeadow
    case .xmbSakuraBloom: return self
    case .xmbRoseTwilight: return .xmbSakuraBloom
    case .xmbCloverField: return self
    case .xmbForestVelvet: return .xmbCloverField
    case .xmbOrchidHaze: return self
    case .xmbVioletNocturne: return .xmbOrchidHaze
    case .xmbTurquoiseLagoon: return self
    case .xmbTidalAbyss: return .xmbTurquoiseLagoon
    case .xmbAzureHorizon: return self
    case .xmbOceanMidnight: return .xmbAzureHorizon
    case .xmbAmethystGlow: return self
    case .xmbPurpleEclipse: return .xmbAmethystGlow
    case .xmbHarvestGold: return self
    case .xmbEmberNight: return .xmbHarvestGold
    case .xmbBronzeAutumn: return self
    case .xmbHearthShadow: return .xmbBronzeAutumn
    case .xmbWinterCoral: return self
    case .xmbCinderRed: return .xmbWinterCoral
    }
  }

  var title: String {
    switch self {
    case .noir:
      return "Black Noir"
    case .multicolor:
      return "Multicolor"
    case .deepTeal:
      return "Deep Teal"
    case .xmbFrostedPearl:
      return "Frosted Pearl"
    case .xmbLunarGraphite:
      return "Lunar Graphite"
    case .xmbGildedSun:
      return "Gilded Sun"
    case .xmbAntiqueDusk:
      return "Antique Dusk"
    case .xmbSpringMeadow:
      return "Spring Meadow"
    case .xmbSageMidnight:
      return "Sage Midnight"
    case .xmbSakuraBloom:
      return "Sakura Bloom"
    case .xmbRoseTwilight:
      return "Rose Twilight"
    case .xmbCloverField:
      return "Clover Field"
    case .xmbForestVelvet:
      return "Forest Velvet"
    case .xmbOrchidHaze:
      return "Orchid Haze"
    case .xmbVioletNocturne:
      return "Violet Nocturne"
    case .xmbTurquoiseLagoon:
      return "Turquoise Lagoon"
    case .xmbTidalAbyss:
      return "Tidal Abyss"
    case .xmbAzureHorizon:
      return "Azure Horizon"
    case .xmbOceanMidnight:
      return "Ocean Midnight"
    case .xmbAmethystGlow:
      return "Amethyst Glow"
    case .xmbPurpleEclipse:
      return "Purple Eclipse"
    case .xmbHarvestGold:
      return "Harvest Gold"
    case .xmbEmberNight:
      return "Ember Night"
    case .xmbBronzeAutumn:
      return "Bronze Autumn"
    case .xmbHearthShadow:
      return "Hearth Shadow"
    case .xmbWinterCoral:
      return "Winter Coral"
    case .xmbCinderRed:
      return "Cinder Red"
    default:
      return rawValue.capitalized
    }
  }
}

// MARK: - ThemePaletteColors

extension ThemePalette {
  var colors: [Color] {
    switch self {
    case .blue:
      return [
        Color(red: 0.12, green: 0.42, blue: 1),
        Color(red: 0.16, green: 0.78, blue: 1),
        .white,
      ]
    case .violet:
      return [
        Color(red: 0.58, green: 0.24, blue: 1),
        Color(red: 0.9, green: 0.26, blue: 0.86),
        Color(red: 0.38, green: 0.12, blue: 0.72),
      ]
    case .cyan:
      return [
        Color(red: 0.05, green: 0.78, blue: 0.95),
        Color(red: 0.14, green: 1, blue: 0.74),
        .white,
      ]
    case .pink:
      return [
        Color(red: 0.95, green: 0.2, blue: 0.62),
        Color(red: 0.72, green: 0.18, blue: 1),
        Color(red: 1, green: 0.62, blue: 0.82),
      ]
    case .gold:
      return [
        Color(red: 1, green: 0.62, blue: 0.12),
        Color(red: 1, green: 0.9, blue: 0.38),
        Color(red: 0.78, green: 0.28, blue: 0.04),
      ]
    case .crimson:
      return [
        Color(red: 0.88, green: 0.06, blue: 0.18),
        Color(red: 1, green: 0.24, blue: 0.38),
        Color(red: 0.34, green: 0.01, blue: 0.06),
      ]
    case .emerald:
      return [
        Color(red: 0.04, green: 0.72, blue: 0.42),
        Color(red: 0.2, green: 1, blue: 0.66),
        Color(red: 0.01, green: 0.2, blue: 0.12),
      ]
    case .midnight:
      return [
        Color(red: 0.08, green: 0.12, blue: 0.42),
        Color(red: 0.16, green: 0.3, blue: 0.72),
        Color(red: 0.02, green: 0.04, blue: 0.14),
      ]
    case .burgundy:
      return [
        Color(red: 0.35, green: 0.03, blue: 0.14),
        Color(red: 0.62, green: 0.08, blue: 0.28),
        Color(red: 0.14, green: 0.01, blue: 0.06),
      ]
    case .graphite:
      return [
        Color(red: 0.32, green: 0.38, blue: 0.48),
        Color(red: 0.62, green: 0.72, blue: 0.82),
        Color(red: 0.07, green: 0.08, blue: 0.11),
      ]
    case .noir:
      return [
        Color(white: 0.58),
        Color(white: 0.16),
        Color(white: 0.9),
      ]
    case .silver:
      return [
        Color(white: 0.78),
        Color(white: 0.95),
        Color(white: 0.36),
      ]
    case .slate:
      return [
        Color(red: 0.28, green: 0.36, blue: 0.46),
        Color(red: 0.46, green: 0.58, blue: 0.7),
        Color(red: 0.08, green: 0.11, blue: 0.16),
      ]
    case .charcoal:
      return [
        Color(white: 0.24),
        Color(white: 0.42),
        Color(white: 0.055),
      ]
    case .navy:
      return [
        Color(red: 0.025, green: 0.09, blue: 0.28),
        Color(red: 0.08, green: 0.24, blue: 0.56),
        Color(red: 0.005, green: 0.018, blue: 0.08),
      ]
    case .deepTeal:
      return [
        Color(red: 0.02, green: 0.34, blue: 0.38),
        Color(red: 0.04, green: 0.66, blue: 0.62),
        Color(red: 0.004, green: 0.09, blue: 0.1),
      ]
    case .lavender:
      return [
        Color(red: 0.62, green: 0.48, blue: 0.94),
        Color(red: 0.86, green: 0.72, blue: 1),
        Color(red: 0.21, green: 0.12, blue: 0.38),
      ]
    case .sunset:
      return [
        Color(red: 1, green: 0.24, blue: 0.28),
        Color(red: 1, green: 0.58, blue: 0.16),
        Color(red: 0.58, green: 0.12, blue: 0.68),
      ]
    case .aurora:
      return [
        Color(red: 0.12, green: 0.9, blue: 0.68),
        Color(red: 0.28, green: 0.44, blue: 1),
        Color(red: 0.74, green: 0.2, blue: 0.94),
      ]
    case .xmbFrostedPearl:
      return Self.xmbColors(start: (197, 197, 197), end: (201, 201, 201))
    case .xmbLunarGraphite:
      return Self.xmbColors(start: (181, 181, 181), end: (0, 0, 0))
    case .xmbGildedSun:
      return Self.xmbColors(start: (203, 158, 13), end: (219, 214, 41))
    case .xmbAntiqueDusk:
      return Self.xmbColors(start: (198, 188, 128), end: (0, 0, 0))
    case .xmbSpringMeadow:
      return Self.xmbColors(start: (142, 190, 40), end: (104, 168, 22))
    case .xmbSageMidnight:
      return Self.xmbColors(start: (152, 170, 113), end: (0, 0, 0))
    case .xmbSakuraBloom:
      return Self.xmbColors(start: (216, 182, 182), end: (231, 66, 117))
    case .xmbRoseTwilight:
      return Self.xmbColors(start: (212, 174, 182), end: (10, 8, 8))
    case .xmbCloverField:
      return Self.xmbColors(start: (19, 108, 19), end: (24, 156, 24))
    case .xmbForestVelvet:
      return Self.xmbColors(start: (48, 118, 48), end: (11, 3, 11))
    case .xmbOrchidHaze:
      return Self.xmbColors(start: (198, 120, 238), end: (103, 77, 161))
    case .xmbVioletNocturne:
      return Self.xmbColors(start: (209, 163, 225), end: (0, 0, 0))
    case .xmbTurquoiseLagoon:
      return Self.xmbColors(start: (0, 167, 146), end: (10, 240, 239))
    case .xmbTidalAbyss:
      return Self.xmbColors(start: (16, 129, 124), end: (17, 0, 0))
    case .xmbAzureHorizon:
      return Self.xmbColors(start: (0, 0, 95), end: (33, 217, 255))
    case .xmbOceanMidnight:
      return Self.xmbColors(start: (20, 159, 176), end: (0, 0, 31))
    case .xmbAmethystGlow:
      return Self.xmbColors(start: (146, 44, 155), end: (217, 98, 236))
    case .xmbPurpleEclipse:
      return Self.xmbColors(start: (116, 0, 153), end: (12, 0, 11))
    case .xmbHarvestGold:
      return Self.xmbColors(start: (227, 151, 15), end: (224, 187, 2))
    case .xmbEmberNight:
      return Self.xmbColors(start: (216, 142, 0), end: (0, 0, 0))
    case .xmbBronzeAutumn:
      return Self.xmbColors(start: (115, 68, 20), end: (154, 118, 47))
    case .xmbHearthShadow:
      return Self.xmbColors(start: (131, 86, 32), end: (18, 20, 17))
    case .xmbWinterCoral:
      return Self.xmbColors(start: (236, 68, 45), end: (214, 63, 43))
    case .xmbCinderRed:
      return Self.xmbColors(start: (157, 59, 44), end: (0, 0, 3))
    case .multicolor:
      return [
        .cyan,
        .pink,
        .purple,
        .green,
        .orange,
        .white,
      ]
    }
  }

  private static func xmbColors(
    start: (red: Int, green: Int, blue: Int),
    end: (red: Int, green: Int, blue: Int)
  ) -> [Color] {
    let startColor = Color(
      red: Double(start.red) / 255,
      green: Double(start.green) / 255,
      blue: Double(start.blue) / 255
    )
    let endColor = Color(
      red: Double(end.red) / 255,
      green: Double(end.green) / 255,
      blue: Double(end.blue) / 255
    )
    let midpoint = Color(
      red: Double(start.red + end.red) / 510,
      green: Double(start.green + end.green) / 510,
      blue: Double(start.blue + end.blue) / 510
    )
    return [endColor, startColor, midpoint]
  }
}

// MARK: - ThemePaletteRelationships

extension ThemePalette {
  var relatedMultiColorPalettes: [ThemePalette] {
    switch self {
    case .blue:
      return [.blue, .cyan, .midnight, .silver]
    case .violet:
      return [.violet, .lavender, .pink, .aurora]
    case .cyan:
      return [.cyan, .deepTeal, .blue, .emerald]
    case .pink:
      return [.pink, .violet, .lavender, .sunset]
    case .gold:
      return [.gold, .sunset, .crimson, .silver]
    case .crimson:
      return [.crimson, .burgundy, .sunset, .gold]
    case .emerald:
      return [.emerald, .deepTeal, .cyan, .aurora]
    case .midnight:
      return [.midnight, .navy, .blue, .slate]
    case .burgundy:
      return [.burgundy, .crimson, .pink, .noir]
    case .graphite:
      return [.graphite, .slate, .silver, .charcoal]
    case .noir:
      return [.noir, .charcoal, .graphite, .silver]
    case .silver:
      return [.silver, .graphite, .slate, .blue]
    case .slate:
      return [.slate, .graphite, .navy, .silver]
    case .charcoal:
      return [.charcoal, .graphite, .noir, .slate]
    case .navy:
      return [.navy, .midnight, .blue, .deepTeal]
    case .deepTeal:
      return [.deepTeal, .cyan, .emerald, .navy]
    case .lavender:
      return [.lavender, .violet, .pink, .aurora]
    case .sunset:
      return [.sunset, .gold, .crimson, .pink]
    case .aurora:
      return [.aurora, .cyan, .emerald, .violet]
    case .xmbFrostedPearl, .xmbLunarGraphite:
      return [.xmbFrostedPearl, .xmbLunarGraphite, .silver, .graphite]
    case .xmbGildedSun, .xmbAntiqueDusk:
      return [.xmbGildedSun, .xmbAntiqueDusk, .gold, .xmbBronzeAutumn]
    case .xmbSpringMeadow, .xmbSageMidnight:
      return [.xmbSpringMeadow, .xmbSageMidnight, .emerald, .deepTeal]
    case .xmbSakuraBloom, .xmbRoseTwilight:
      return [.xmbSakuraBloom, .xmbRoseTwilight, .pink, .burgundy]
    case .xmbCloverField, .xmbForestVelvet:
      return [.xmbCloverField, .xmbForestVelvet, .emerald, .charcoal]
    case .xmbOrchidHaze, .xmbVioletNocturne:
      return [.xmbOrchidHaze, .xmbVioletNocturne, .violet, .lavender]
    case .xmbTurquoiseLagoon, .xmbTidalAbyss:
      return [.xmbTurquoiseLagoon, .xmbTidalAbyss, .cyan, .deepTeal]
    case .xmbAzureHorizon, .xmbOceanMidnight:
      return [.xmbAzureHorizon, .xmbOceanMidnight, .blue, .navy]
    case .xmbAmethystGlow, .xmbPurpleEclipse:
      return [.xmbAmethystGlow, .xmbPurpleEclipse, .pink, .violet]
    case .xmbHarvestGold, .xmbEmberNight:
      return [.xmbHarvestGold, .xmbEmberNight, .gold, .crimson]
    case .xmbBronzeAutumn, .xmbHearthShadow:
      return [.xmbBronzeAutumn, .xmbHearthShadow, .gold, .charcoal]
    case .xmbWinterCoral, .xmbCinderRed:
      return [.xmbWinterCoral, .xmbCinderRed, .crimson, .burgundy]
    case .multicolor:
      return [.multicolor, .cyan, .pink, .gold]
    }
  }

  // Returns a static palette color, or rotates through hues for Multicolor.
  func animatedColor(index: Int, time: TimeInterval) -> Color {
    guard self == .multicolor else {
      return colors[index % colors.count]
    }
    let hue = (time * 0.055 + Double(index) * 0.137)
      .truncatingRemainder(dividingBy: 1)
    return Color(
      hue: hue,
      saturation: 0.86,
      brightness: 1
    )
  }
}

// MARK: - ThemePaletteTarget

enum ThemePaletteTarget: String, CaseIterable, Identifiable {
  case shared
  case ribbons
  case particles

  var id: String { rawValue }

  var title: String {
    switch self {
    case .shared:
      return "Shared Themes"
    case .ribbons:
      return "Ribbons"
    case .particles:
      return "Dynamic Settings"
    }
  }
}

// MARK: - SavedPaletteColor

struct SavedPaletteColor: Codable, Hashable, Identifiable {
  let hex: String
  let gradientHexes: [String]?
  let darkEffectHex: String?
  let darkEffectGradientHexes: [String]?
  let darkEffectEnabled: Bool?

  var id: String {
    ([hex] + (gradientHexes ?? []) + [darkEffectHex ?? ""]
      + (darkEffectGradientHexes ?? [])
      + [darkEffectEnabled.map(String.init) ?? ""]).joined(separator: "-")
  }

  var isGradient: Bool {
    guard let gradientHexes else { return false }
    return gradientHexes.count > 1
  }

  var gradientColors: [Color] {
    if let gradientHexes, !gradientHexes.isEmpty {
      return gradientHexes.map(Self.color(from:))
    }

    let components = rgbComponents
    return [
      Color(
        red: components.red * 0.34,
        green: components.green * 0.34,
        blue: components.blue * 0.34
      ),
      Color(
        red: components.red,
        green: components.green,
        blue: components.blue
      ),
      Color(
        red: components.red + (1 - components.red) * 0.38,
        green: components.green + (1 - components.green) * 0.38,
        blue: components.blue + (1 - components.blue) * 0.38
      ),
    ]
  }

  var darkEffectColor: Color? {
    darkEffectColor(at: 0)
  }

  var usesDarkEffect: Bool {
    darkEffectEnabled ?? true
  }

  func darkEffectColor(at progress: Double) -> Color? {
    let hexes = darkEffectGradientHexes ?? darkEffectHex.map { [$0] }
    guard let hexes, let firstHex = hexes.first else { return nil }
    guard hexes.count > 1 else { return Self.color(from: firstHex) }

    let position = min(1, max(0, progress)) * Double(hexes.count - 1)
    let lowerIndex = min(hexes.count - 1, Int(floor(position)))
    let upperIndex = min(hexes.count - 1, lowerIndex + 1)
    let amount = position - Double(lowerIndex)
    let lower = Self.rgbComponents(for: hexes[lowerIndex])
    let upper = Self.rgbComponents(for: hexes[upperIndex])
    return Color(
      red: lower.red + (upper.red - lower.red) * amount,
      green: lower.green + (upper.green - lower.green) * amount,
      blue: lower.blue + (upper.blue - lower.blue) * amount
    )
  }

  // Stores a SwiftUI color as an RGB hex value that can be saved in AppStorage.
  init?(color: Color) {
    guard let hex = Self.hexValue(for: color) else { return nil }
    self.hex = hex
    gradientHexes = nil
    darkEffectHex = nil
    darkEffectGradientHexes = nil
    darkEffectEnabled = nil
  }

  init?(
    colors: [Color],
    darkEffectColors: [Color]? = nil,
    darkEffectEnabled: Bool = true
  ) {
    let hexes = colors.compactMap(Self.hexValue(for:))
    guard hexes.count == colors.count, hexes.count > 1 else { return nil }
    let darkEffectHexes = darkEffectColors?.compactMap(Self.hexValue(for:))
    guard darkEffectColors == nil || darkEffectHexes?.count == darkEffectColors?.count else {
      return nil
    }
    hex = hexes[0]
    gradientHexes = hexes
    darkEffectHex = darkEffectHexes?.first
    darkEffectGradientHexes = darkEffectHexes
    self.darkEffectEnabled = darkEffectEnabled
  }

  // Resolves either a saved gradient stop or the generated variants of a solid color.
  func animatedColor(index: Int, time _: TimeInterval) -> Color {
    let colors = gradientColors
    return colors[index % colors.count]
  }

  private var rgbComponents: (red: Double, green: Double, blue: Double) {
    Self.rgbComponents(for: hex)
  }

  private static func color(from hex: String) -> Color {
    let components = rgbComponents(for: hex)
    return Color(
      red: components.red,
      green: components.green,
      blue: components.blue
    )
  }

  private static func rgbComponents(
    for hex: String
  ) -> (red: Double, green: Double, blue: Double) {
    let value = UInt64(hex.dropFirst(), radix: 16) ?? 0
    return (
      Double((value >> 16) & 0xFF) / 255,
      Double((value >> 8) & 0xFF) / 255,
      Double(value & 0xFF) / 255
    )
  }

  private static func hexValue(for color: Color) -> String? {
    let uiColor = UIColor(color)
    var red: CGFloat = 0
    var green: CGFloat = 0
    var blue: CGFloat = 0
    var alpha: CGFloat = 0
    guard uiColor.getRed(&red, green: &green, blue: &blue, alpha: &alpha) else {
      return nil
    }
    return String(
      format: "#%02X%02X%02X",
      Int(red * 255),
      Int(green * 255),
      Int(blue * 255)
    )
  }
}
