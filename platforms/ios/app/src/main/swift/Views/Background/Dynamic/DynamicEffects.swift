// DynamicEffects.swift — Shared dynamic background effects
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit

struct OrbTrailBezierSegment {
  let start: CGPoint
  let end: CGPoint
  let control1: CGPoint
  let control2: CGPoint
}

enum OrbTrailGeometry {
  static func segment(
    points: [CGPoint],
    index: Int,
    maximumTangentLength: CGFloat
  ) -> OrbTrailBezierSegment {
    let start = points[index]
    let end = points[index + 1]
    let startTangent = tangent(
      points: points,
      index: index,
      maximumLength: maximumTangentLength
    )
    let endTangent = tangent(
      points: points,
      index: index + 1,
      maximumLength: maximumTangentLength
    )

    return OrbTrailBezierSegment(
      start: start,
      end: end,
      control1: CGPoint(
        x: start.x + startTangent.dx / 3,
        y: start.y + startTangent.dy / 3
      ),
      control2: CGPoint(
        x: end.x - endTangent.dx / 3,
        y: end.y - endTangent.dy / 3
      )
    )
  }

  private static func tangent(
    points: [CGPoint],
    index: Int,
    maximumLength: CGFloat
  ) -> CGVector {
    let point = points[index]
    let previous = points[max(0, index - 1)]
    let following = points[min(points.count - 1, index + 1)]
    let raw = CGVector(
      dx: (following.x - previous.x) * 0.5,
      dy: (following.y - previous.y) * 0.5
    )
    let rawLength = hypot(raw.dx, raw.dy)
    guard rawLength > 0 else { return .zero }

    let adjacentLengths = [
      hypot(point.x - previous.x, point.y - previous.y),
      hypot(following.x - point.x, following.y - point.y),
    ].filter { $0 > 0 }
    guard let shortestAdjacent = adjacentLengths.min() else { return .zero }

    let allowedLength = min(maximumLength, shortestAdjacent * 1.25)
    let scale = min(1, allowedLength / rawLength)
    return CGVector(dx: raw.dx * scale, dy: raw.dy * scale)
  }
}

// MARK: - DynamicBackgroundEffects

struct DynamicBackgroundTheme: Equatable {
  let sharedPalette: ThemePalette
  let sharedCustomColor: SavedPaletteColor?
  let sharedMultiColor: ThemeMultiColorSelection
  let ribbonPalette: ThemePalette
  let ribbonCustomColor: SavedPaletteColor?
  let ribbonMultiColor: ThemeMultiColorSelection
  let particleSettings: DynamicParticleSettings

  var usesDynamicSharedPalette: Bool {
    if let sharedCustomColor {
      return sharedCustomColor.isGradient
    }

    if sharedMultiColor.isEnabled {
      return !sharedMultiColor.palettes.isEmpty
        && sharedMultiColor.palettes.allSatisfy {
          ThemePalette.primaryPalettes.contains($0)
        }
    }

    return ThemePalette.primaryPalettes.contains(sharedPalette)
  }

  // Resolves the base palette used for background gradients and broad color fields.
  func sharedColor(index: Int, time: TimeInterval) -> Color {
    if sharedMultiColor.isEnabled {
      return sharedMultiColor.color(
        index: index,
        time: time,
        settings: particleSettings
      )
    }
    return sharedCustomColor?.animatedColor(index: index, time: time)
      ?? sharedPalette.animatedColor(index: index, time: time)
  }

  // Resolves the accent palette used for ribbons and foreground details.
  func ribbonColor(index: Int, time: TimeInterval) -> Color {
    if ribbonMultiColor.isEnabled {
      return ribbonMultiColor.color(
        index: index,
        time: time,
        settings: particleSettings
      )
    }
    return ribbonCustomColor?.animatedColor(index: index, time: time)
      ?? ribbonPalette.animatedColor(index: index, time: time)
  }

  // Keeps the default PS4 blue base while still allowing Shared Themes to override it.
  func playStation4BaseColor(index: Int, time: TimeInterval) -> Color {
    if sharedMultiColor.isEnabled {
      return sharedMultiColor.color(
        index: index,
        time: time,
        settings: particleSettings
      )
    }
    if let sharedCustomColor {
      return sharedCustomColor.animatedColor(index: index, time: time)
    }
    if sharedPalette == .blue {
      switch index % 3 {
      case 0:
        return Color(red: 0.004, green: 0.025, blue: 0.16)
      case 1:
        return Color(red: 0.015, green: 0.2, blue: 0.58)
      default:
        return Color(red: 0.12, green: 0.42, blue: 1)
      }
    }
    return sharedPalette.animatedColor(index: index, time: time)
  }

  var paletteDarkEffectStrength: Double {
    guard
      !particleSettings.disablesDarkPaletteEffects,
      sharedCustomColor?.usesDarkEffect != false
    else {
      return 0
    }
    return min(2, max(0, particleSettings.paletteDarkEffectIntensity))
  }

  func paletteDarkEffectColor(at progress: Double) -> Color {
    sharedCustomColor?.darkEffectColor(at: progress) ?? .black
  }

  var sharedPaletteGradientTilt: Double {
    particleSettings.sharedPaletteGradientTilt
  }

  var sharedPaletteGradientCurvature: Double {
    particleSettings.sharedPaletteGradientCurvature
  }

  func sharedGradientPoints(
    from startPoint: UnitPoint,
    to endPoint: UnitPoint
  ) -> (start: UnitPoint, end: UnitPoint) {
    transformedGradientPoints(
      from: startPoint,
      to: endPoint,
      degrees: sharedPaletteGradientTilt,
      offsetX: particleSettings.sharedPaletteGradientOffsetX,
      offsetY: particleSettings.sharedPaletteGradientOffsetY,
      width: particleSettings.sharedPaletteGradientWidth
    )
  }

  // Produces an opaque palette stop shaded toward the customizable dark effect.
  func paletteBackgroundColor(_ color: Color, darkness: Double) -> Color {
    blendPaletteEffectColor(
      color,
      paletteDarkEffectColor(at: darkness),
      amount: min(1, max(0, darkness * paletteDarkEffectStrength))
    )
  }

  // Applies the same shared dark effect to vignettes without altering palette stops.
  func paletteDarkOverlay(opacity: Double) -> Color {
    paletteDarkEffectColor(at: opacity).opacity(
      min(1, max(0, opacity * paletteDarkEffectStrength))
    )
  }

  // Keeps default highlights white until the Ribbons palette is changed.
  func highlightColor(index: Int, time: TimeInterval) -> Color {
    if ribbonMultiColor.isEnabled || ribbonCustomColor != nil || ribbonPalette != .cyan {
      return ribbonColor(index: index, time: time)
    }
    return .white
  }
}

private func transformedGradientPoints(
  from startPoint: UnitPoint,
  to endPoint: UnitPoint,
  degrees: Double,
  offsetX: Double,
  offsetY: Double,
  width: Double
) -> (start: UnitPoint, end: UnitPoint) {
  let radians = degrees * .pi / 180
  let cosine = CGFloat(cos(radians))
  let sine = CGFloat(sin(radians))
  let scale = CGFloat(max(0.05, width))
  let center = CGPoint(
    x: (startPoint.x + endPoint.x) * 0.5 + CGFloat(offsetX),
    y: (startPoint.y + endPoint.y) * 0.5 + CGFloat(offsetY)
  )
  let halfVector = CGPoint(
    x: (endPoint.x - startPoint.x) * 0.5 * scale,
    y: (endPoint.y - startPoint.y) * 0.5 * scale
  )
  let rotatedHalfVector = CGPoint(
    x: halfVector.x * cosine - halfVector.y * sine,
    y: halfVector.x * sine + halfVector.y * cosine
  )

  return (
    UnitPoint(
      x: center.x - rotatedHalfVector.x,
      y: center.y - rotatedHalfVector.y
    ),
    UnitPoint(
      x: center.x + rotatedHalfVector.x,
      y: center.y + rotatedHalfVector.y
    )
  )
}

private func blendPaletteEffectColor(
  _ first: Color,
  _ second: Color,
  amount: Double
) -> Color {
  let progress = min(1, max(0, amount))
  guard progress > 0 else { return first }

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
    return first
  }

  return Color(
    red: Double(firstRed + (secondRed - firstRed) * progress),
    green: Double(firstGreen + (secondGreen - firstGreen) * progress),
    blue: Double(firstBlue + (secondBlue - firstBlue) * progress),
    opacity: Double(firstAlpha + (secondAlpha - firstAlpha) * progress)
  )
}

// MARK: - DynamicBackgroundUtilities

enum DynamicBackgroundCoding {
  static func decode<Value: Decodable>(
    _ type: Value.Type,
    from data: Data
  ) -> Value? {
    try? JSONDecoder().decode(type, from: data)
  }

  static func decode<Value: Decodable>(
    _ type: Value.Type,
    from json: String
  ) -> Value? {
    guard let data = json.data(using: .utf8) else { return nil }
    return decode(type, from: data)
  }

  static func encode<Value: Encodable>(_ value: Value) -> Data? {
    try? JSONEncoder().encode(value)
  }

  static func encodeJSONString<Value: Encodable>(_ value: Value) -> String? {
    guard let data = encode(value) else { return nil }
    return String(data: data, encoding: .utf8)
  }
}

enum DynamicBackgroundMath {
  static func clamp(_ value: Double, to range: ClosedRange<Double>) -> Double {
    min(range.upperBound, max(range.lowerBound, value))
  }

  static func unit(_ value: Double) -> Double {
    value - floor(value)
  }

  static func seededUnit(index: Int, salt: Double) -> Double {
    unit(sin((Double(index) + 1) * salt) * 43_758.5453)
  }

  static func smoothstep(
    from lowerBound: Double = 0,
    to upperBound: Double = 1,
    value: Double
  ) -> Double {
    let progress = clamp(
      (value - lowerBound) / (upperBound - lowerBound),
      to: 0...1
    )
    return progress * progress * (3 - 2 * progress)
  }
}

enum DynamicBackgroundGeometry {
  // Converts sampled points into a smooth Catmull-Rom-style cubic path.
  static func smoothCurve(through points: [CGPoint]) -> Path {
    guard let first = points.first else { return Path() }

    var path = Path()
    path.move(to: first)

    for index in 0..<(points.count - 1) {
      let previous = index > 0 ? points[index - 1] : points[index]
      let current = points[index]
      let next = points[index + 1]
      let following = index + 2 < points.count ? points[index + 2] : next
      let control1 = CGPoint(
        x: current.x + (next.x - previous.x) / 6,
        y: current.y + (next.y - previous.y) / 6
      )
      let control2 = CGPoint(
        x: next.x - (following.x - current.x) / 6,
        y: next.y - (following.y - current.y) / 6
      )

      path.addCurve(
        to: next,
        control1: control1,
        control2: control2
      )
    }

    return path
  }
}

// MARK: - DynamicParticleOverlay

struct DynamicParticleOverlay: View {
  let theme: DynamicBackgroundTheme
  @Environment(\.menuBackgroundSessionStart) private var menuBackgroundSessionStart

  @ViewBuilder
  var body: some View {
    if settings.style == .xmbMart {
      PlayStation3XMBMartMetalSurface(
        settings: settings.playStation3XMB,
        theme: theme,
        sessionStartTime: menuBackgroundSessionStart.timeIntervalSinceReferenceDate,
        renderMode: .particlesOnly,
        particleControls: martParticleControls
      )
      .ignoresSafeArea()
    } else {
      TimelineView(.animation(minimumInterval: 1 / 30)) { timeline in
        let time = timeline.date.timeIntervalSinceReferenceDate
        Canvas { context, size in
          context.blendMode = .plusLighter
          drawParticles(
            context: &context,
            size: size,
            time: time
          )
        }
        .ignoresSafeArea()
      }
    }
  }

  private var settings: DynamicParticleSettings {
    theme.particleSettings
  }

  private var directedSpeed: Double {
    settings.speed * settings.speedDirection * 2
  }

  private var martParticleControls: PlayStation3XMBMartParticleControls {
    PlayStation3XMBMartParticleControls(
      amount: settings.amount * settings.verticalDensity,
      speed: settings.speed,
      direction: settings.speedDirection,
      dispersion: settings.dispersion,
      verticalSpread: settings.verticalSpread,
      outerDispersion: settings.outerDispersion,
      verticalLevel: settings.verticalLevel,
      size: settings.size,
      brightness: settings.brightness,
      opacity: settings.opacity,
      depthVariation: settings.depthVariation
    )
  }

  // Draws the selected particle family with the user's shared controls.
  private func drawParticles(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval
  ) {
    switch settings.style {
    case .xmb3:
      drawXMBParticles(context: &context, size: size, time: time, baseCount: 150)
    case .xmbMart:
      return
    case .ps1Dust:
      drawPixelDust(context: &context, size: size, time: time, baseCount: 90)
    case .ps4Glow:
      drawGlowParticles(context: &context, size: size, time: time, baseCount: 56)
    case .ps5Drift:
      drawDriftParticles(context: &context, size: size, time: time, baseCount: 96)
    case .mixed:
      drawXMBParticles(context: &context, size: size, time: time, baseCount: 92)
      drawPixelDust(context: &context, size: size, time: time, baseCount: 42)
      drawGlowParticles(context: &context, size: size, time: time, baseCount: 28)
      drawDriftParticles(context: &context, size: size, time: time, baseCount: 46)
    }
  }

  // Draws XMB3-style sparkles around the selected vertical band.
  private func drawXMBParticles(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    baseCount: Int
  ) {
    for index in 0..<particleCount(baseCount) {
      let seed = DynamicBackgroundMath.unit(sin(Double(index + 1) * 12.9898) * 43_758.5453)
      let secondarySeed = DynamicBackgroundMath.unit(sin(Double(index + 7) * 78.233) * 9_631.417)
      let speed = (0.006 + Double(index % 7) * 0.0011) * directedSpeed
      let progress = DynamicBackgroundMath.unit(seed + time * speed)
      let center = particleCenter(
        size: size,
        progress: progress,
        seed: seed,
        time: time
      )
      let spread =
        (CGFloat(secondarySeed) - 0.5)
        * size.height
        * (0.18 + CGFloat(seed) * 0.2)
        * CGFloat(settings.dispersion)
        * CGFloat(settings.verticalSpread)
      let twinkle = particleTwinkle(seed: seed, secondarySeed: secondarySeed, time: time)
      let diameter =
        (CGFloat(0.7 + Double(index % 5) * 0.34) + CGFloat(twinkle) * 1.35)
        * CGFloat(settings.size)
        * particleDepthScale(seed: seed)

      fillParticle(
        context: &context,
        rect: CGRect(
          x: CGFloat(progress) * size.width - diameter / 2,
          y: center + spread + outerVerticalOffset(seed: secondarySeed, size: size)
            + CGFloat(sin(time * 0.24 + seed * 18)) * 7 - diameter / 2,
          width: diameter,
          height: diameter
        ),
        color: particleColor(index: index, time: time),
        opacity: (index.isMultiple(of: 8) ? 0.52 + twinkle * 0.48 : 0.18 + twinkle * 0.3)
          * particleDepthOpacity(seed: seed)
      )
    }
  }

  // Draws small square particles based on the PS1 pixel dust.
  private func drawPixelDust(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    baseCount: Int
  ) {
    for index in 0..<particleCount(baseCount) {
      let seed = Double(index) * 1.731
      let progress = DynamicBackgroundMath.unit(
        Double(index) * 0.071
          + time * (0.008 + Double(index % 4) * 0.002) * directedSpeed
      )
      let x =
        CGFloat(
          sin(seed * 2.17 + time * 0.02 * settings.drift * directedSpeed)
            * 0.5 + 0.5
        )
        * size.width
      let y =
        size.height * CGFloat(settings.verticalLevel)
        + CGFloat(0.5 - progress) * size.height * 0.7 * CGFloat(settings.dispersion)
        * CGFloat(settings.verticalSpread)
        + outerVerticalOffset(seed: DynamicBackgroundMath.unit(seed * 0.731), size: size)
      let side =
        CGFloat(index.isMultiple(of: 9) ? 2.2 : 1.1)
        * CGFloat(settings.size)
        * particleDepthScale(seed: seed)

      context.fill(
        Path(
          CGRect(
            x: x - side / 2,
            y: y - side / 2,
            width: side,
            height: side
          )
        ),
        with: .color(
          particleColor(index: index, time: time).opacity(
            settings.brightness * settings.opacity * particleDepthOpacity(seed: seed)
          )
        )
      )
    }
  }

  // Draws soft PS4-style floating particles around the selected band.
  private func drawGlowParticles(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    baseCount: Int
  ) {
    for index in 0..<particleCount(baseCount) {
      let seed = Double(index) * 1.618
      let x =
        CGFloat(sin(time * 0.035 * directedSpeed + seed) * 0.5 + 0.5)
        * size.width
      let y =
        size.height * CGFloat(settings.verticalLevel)
        + CGFloat(cos(time * 0.026 * directedSpeed + seed * 1.37))
        * size.height
        * 0.28
        * CGFloat(settings.dispersion)
        * CGFloat(settings.verticalSpread)
        + outerVerticalOffset(seed: DynamicBackgroundMath.unit(seed * 0.619), size: size)
      let diameter =
        CGFloat(1.2 + Double(index % 4) * 0.55)
        * CGFloat(settings.size)
        * particleDepthScale(seed: seed)

      fillParticle(
        context: &context,
        rect: CGRect(
          x: x - diameter / 2,
          y: y - diameter / 2,
          width: diameter,
          height: diameter
        ),
        color: particleColor(index: index, time: time),
        opacity: (index.isMultiple(of: 5) ? 0.52 : 0.34)
          * particleDepthOpacity(seed: seed)
      )
    }
  }

  // Draws PS5-style drifting points across the whole screen width.
  private func drawDriftParticles(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    baseCount: Int
  ) {
    for index in 0..<particleCount(baseCount) {
      let seed = Double(index) * 1.618
      let progress = DynamicBackgroundMath.unit(
        Double(index) * 0.047
          + time * (0.006 + Double(index % 5) * 0.0015) * directedSpeed
      )
      let x =
        CGFloat(progress) * size.width
        + CGFloat(sin(time * 0.16 * directedSpeed + seed))
        * 24 * CGFloat(settings.drift)
      let y =
        size.height * CGFloat(settings.verticalLevel)
        + CGFloat(cos(seed * 1.37)) * size.height * 0.42 * CGFloat(settings.dispersion)
        * CGFloat(settings.verticalSpread)
        + outerVerticalOffset(seed: DynamicBackgroundMath.unit(seed * 0.487), size: size)
        + CGFloat(sin(time * 0.1 * directedSpeed + seed))
        * 38 * CGFloat(settings.drift)
      let diameter =
        CGFloat(0.8 + Double(index % 4) * 0.52)
        * CGFloat(settings.size)
        * particleDepthScale(seed: seed)

      fillParticle(
        context: &context,
        rect: CGRect(
          x: x - diameter / 2,
          y: y - diameter / 2,
          width: diameter,
          height: diameter
        ),
        color: particleColor(index: index, time: time),
        opacity: (index.isMultiple(of: 7) ? 0.58 : 0.28)
          * particleDepthOpacity(seed: seed)
      )
    }
  }

  // Draws one particle ellipse with the shared brightness multiplier.
  private func fillParticle(
    context: inout GraphicsContext,
    rect: CGRect,
    color: Color,
    opacity: Double
  ) {
    context.fill(
      Path(ellipseIn: rect),
      with: .color(color.opacity(opacity * settings.brightness * settings.opacity))
    )
  }

  private func particleCenter(
    size: CGSize,
    progress: Double,
    seed: Double,
    time: TimeInterval
  ) -> CGFloat {
    let normalizedY =
      CGFloat(settings.verticalLevel)
      + 0.095
      * CGFloat(
        sin(progress * .pi * 2.1 - time * 0.11 * directedSpeed + seed * 5)
      )
      * CGFloat(settings.dispersion)
      * CGFloat(settings.verticalSpread)
    return size.height * normalizedY
  }

  private func outerVerticalOffset(seed: Double, size: CGSize) -> CGFloat {
    guard settings.outerDispersion > 0 else { return 0 }

    let centeredSeed = CGFloat(DynamicBackgroundMath.unit(seed) * 2 - 1)
    let direction: CGFloat = centeredSeed < 0 ? -1 : 1
    let edgeBias = pow(abs(centeredSeed), 0.45)
    return direction * edgeBias * size.height * 0.16 * CGFloat(settings.outerDispersion)
  }

  private func particleDepthScale(seed: Double) -> CGFloat {
    let variation = (DynamicBackgroundMath.unit(seed * 1.371) - 0.5) * settings.depthVariation
    return CGFloat(max(0.25, 1 + variation))
  }

  private func particleDepthOpacity(seed: Double) -> Double {
    let scale = Double(particleDepthScale(seed: seed))
    return min(1.35, max(0.35, 0.72 + scale * 0.28))
  }

  private func particleTwinkle(
    seed: Double,
    secondarySeed: Double,
    time: TimeInterval
  ) -> Double {
    pow(
      max(0, sin(time * (0.9 + seed) * settings.speed + secondarySeed * 14)),
      3
    ) * settings.twinkle
  }

  private func particleColor(index: Int, time: TimeInterval) -> Color {
    index.isMultiple(of: 8)
      ? theme.highlightColor(index: index, time: time)
      : theme.ribbonColor(index: index, time: time)
  }

  private func particleCount(_ baseCount: Int) -> Int {
    min(
      900,
      max(
        0,
        Int(Double(baseCount) * settings.amount * settings.verticalDensity)
      )
    )
  }
}
