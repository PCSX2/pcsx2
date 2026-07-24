// DynamicBackgrounds.swift — SwiftUI dynamic library backgrounds
// SPDX-License-Identifier: GPL-3.0+

import Foundation
@preconcurrency import MetalKit
import SwiftUI
import UIKit

// MARK: - DynamicBackgroundStyle

enum DynamicBackgroundStyle: String, CaseIterable, Identifiable, Codable {
  case multicolorAmbient = "ambient"
  case lightSpeed = "space"
  case spatialRetro = "ps1"
  case towersOrbs = "towers"
  case playStation2Menu = "towersSky"
  case playStation3XMBByMart = "xmbMart"
  case faceButtons
  case playStationPortableBlur = "xmb"
  case playStation3Splines = "xmb3"
  case playStation4Particles = "xmb4"
  case playStation4Waves = "waves"
  case playStationRibbons = "ps5"

  var id: String { rawValue }

  var title: String {
    switch self {
    case .multicolorAmbient:
      return "Multicolor Ambient"
    case .lightSpeed:
      return "Light Speed"
    case .spatialRetro:
      return "Spatial Retro"
    case .towersOrbs:
      return "Towers Orbs"
    case .playStation2Menu:
      return "PlayStation 2 Menu by Henyckma"
    case .playStation3XMBByMart:
      return "PlayStation 3 XMB by Mart"
    case .faceButtons:
      return "Face Buttons"
    case .playStationPortableBlur:
      return "PlayStation Portable Blur"
    case .playStation3Splines:
      return "PlayStation 3 Splines"
    case .playStation4Particles:
      return "PlayStation 4 Particles"
    case .playStation4Waves:
      return "PlayStation 4 Waves"
    case .playStationRibbons:
      return "PlayStation Ribbons"
    }
  }

  var systemImage: String {
    switch self {
    case .multicolorAmbient:
      return "circle.hexagongrid.fill"
    case .lightSpeed:
      return "forward.end.fill"
    case .spatialRetro:
      return "square.grid.3x3.fill"
    case .towersOrbs:
      return "circle.grid.2x2.fill"
    case .playStation2Menu:
      return "cube.fill"
    case .playStation3XMBByMart:
      return "water.waves"
    case .faceButtons:
      return "gamecontroller.fill"
    case .playStationPortableBlur:
      return "drop.fill"
    case .playStation3Splines:
      return "waveform.path"
    case .playStation4Particles:
      return "sparkles"
    case .playStation4Waves:
      return "water.waves"
    case .playStationRibbons:
      return "scribble.variable"
    }
  }

  var settingsExpansionIdentifier: String {
    // Keep the pre-rename values so saved disclosure state survives app updates.
    let legacyName: String

    switch self {
    case .multicolorAmbient: legacyName = "Orbit Ambient"
    case .lightSpeed: legacyName = "Space"
    case .spatialRetro: legacyName = "PS1 Style"
    case .towersOrbs: legacyName = "PS2 Towers"
    case .playStation2Menu: legacyName = "Towers Sky"
    case .playStation3XMBByMart: legacyName = "PlayStation 3 XMB by Mart"
    case .faceButtons: legacyName = "Face Buttons"
    case .playStationPortableBlur: legacyName = "XMB"
    case .playStation3Splines: legacyName = "XMB3"
    case .playStation4Particles: legacyName = "XMB4"
    case .playStation4Waves: legacyName = "PS4 Waves"
    case .playStationRibbons: legacyName = "PS5 Style"
    }

    return "background.\(legacyName)"
  }

  // Creates the selected animated background with shared base colors and ribbon accents.
  @ViewBuilder
  func makeBackground(theme: DynamicBackgroundTheme) -> some View {
    ZStack {
      switch self {
      case .multicolorAmbient:
        MulticolorAmbientBackground(theme: theme)
      case .lightSpeed:
        LightSpeedBackground(theme: theme)
      case .spatialRetro:
        SpatialRetroBackground(theme: theme)
      case .towersOrbs:
        TowersOrbsBackground(theme: theme)
      case .playStation2Menu:
        PlayStation2MenuBackground(theme: theme)
      case .playStation3XMBByMart:
        PlayStation3XMBByMartBackground(theme: theme)
      case .faceButtons:
        FaceButtonsBackground(theme: theme)
      case .playStationPortableBlur:
        PlayStationPortableBlurBackground(theme: theme)
      case .playStation3Splines:
        PlayStation3SplinesBackground(theme: theme, ribbonSizing: .playStation3)
      case .playStation4Particles:
        PlayStation4ParticlesBackground(theme: theme)
      case .playStation4Waves:
        PlayStation4WavesBackground(theme: theme)
      case .playStationRibbons:
        PlayStationRibbonsBackground(theme: theme)
      }

      if theme.particleSettings.isEnabled {
        DynamicParticleOverlay(theme: theme)
      }

      if self != .faceButtons && theme.particleSettings.faceButtonsEnabled {
        FaceButtonsBackground(
          theme: theme,
          showsBackdrop: false,
          showsLightBands: false
        )
      }
    }
  }
}

// MARK: - MulticolorAmbientBackground

struct MulticolorAmbientBackground: View {
  let theme: DynamicBackgroundTheme

  var body: some View {
    TimelineView(.animation(minimumInterval: 1 / 30)) { timeline in
      let time =
        timeline.date.timeIntervalSinceReferenceDate
        * theme.particleSettings.backgrounds.multicolorAmbientSpeed
      let primaryColor = theme.sharedColor(index: 0, time: time)
      let secondaryColor = theme.ribbonColor(index: 1, time: time)
      let darkColor = theme.sharedColor(index: 2, time: time)
      let sharedGradient = theme.sharedGradientPoints(
        from: .topLeading,
        to: .bottomTrailing
      )

      ZStack {
        PaletteGradientField(
          colors: [
            theme.paletteBackgroundColor(primaryColor, darkness: 0.91),
            theme.paletteBackgroundColor(darkColor, darkness: 0.76),
            theme.paletteBackgroundColor(primaryColor, darkness: 0.84),
          ],
          startPoint: sharedGradient.start,
          endPoint: sharedGradient.end,
          curvature: theme.sharedPaletteGradientCurvature
        )

        Circle()
          .fill(primaryColor.opacity(0.34))
          .frame(width: 330, height: 330)
          .blur(radius: 80)
          .offset(
            x: CGFloat(sin(time * 0.21)) * 150 - 100,
            y: CGFloat(cos(time * 0.17)) * 100 - 160
          )

        Circle()
          .fill(secondaryColor.opacity(0.24))
          .frame(width: 300, height: 300)
          .blur(radius: 90)
          .offset(
            x: CGFloat(cos(time * 0.19)) * 170 + 130,
            y: CGFloat(sin(time * 0.15)) * 150 + 120
          )

        Circle()
          .fill(.white.opacity(0.12))
          .frame(width: 250, height: 250)
          .blur(radius: 70)
          .offset(
            x: CGFloat(cos(time * 0.13)) * 110,
            y: CGFloat(sin(time * 0.23)) * 130 + 180
          )

        Canvas { context, size in
          let particleColor = theme.ribbonColor(index: 2, time: time)
          for index in 0..<26 {
            let phase = Double(index) * 1.73
            let x = (sin(time * 0.07 + phase) * 0.5 + 0.5) * size.width
            let y = (cos(time * 0.05 + phase * 1.4) * 0.5 + 0.5) * size.height
            let rect = CGRect(x: x, y: y, width: 1.5, height: 1.5)
            context.fill(Path(ellipseIn: rect), with: .color(particleColor.opacity(0.22)))
          }
        }

        Rectangle()
          .fill(theme.paletteDarkOverlay(opacity: 0.08))
      }
      .ignoresSafeArea()
    }
  }
}

// MARK: - LightSpeedBackground

struct LightSpeedBackground: View {
  let theme: DynamicBackgroundTheme

  var body: some View {
    TimelineView(.animation(minimumInterval: 1 / 30)) { timeline in
      let time =
        timeline.date.timeIntervalSinceReferenceDate
        * theme.particleSettings.backgrounds.lightSpeedMotionSpeed
      let deepColor = theme.sharedColor(index: 0, time: time)
      let primaryColor = theme.sharedColor(index: 1, time: time)
      let sharedGradient = theme.sharedGradientPoints(
        from: .topLeading,
        to: .bottomTrailing
      )

      ZStack {
        theme.paletteBackgroundColor(deepColor, darkness: 1)

        PaletteGradientField(
          colors: [
            theme.paletteBackgroundColor(deepColor, darkness: 1),
            theme.paletteBackgroundColor(deepColor, darkness: 0.86),
            theme.paletteBackgroundColor(primaryColor, darkness: 0.95),
            theme.paletteBackgroundColor(primaryColor, darkness: 1),
          ],
          startPoint: sharedGradient.start,
          endPoint: sharedGradient.end,
          curvature: theme.sharedPaletteGradientCurvature
        )

        Canvas { context, size in
          let vanishingPoint = vanishingPoint(size: size, time: time)

          drawBackgroundNebulas(
            context: &context,
            size: size,
            time: time
          )
          drawStarParticles(
            context: &context,
            size: size,
            time: time
          )
          drawLightSpeedStreaks(
            context: &context,
            size: size,
            time: time,
            vanishingPoint: vanishingPoint
          )
        }

        LinearGradient(
          colors: [
            theme.paletteDarkOverlay(opacity: 0.12),
            .clear,
            theme.paletteDarkOverlay(opacity: 0.18),
          ],
          startPoint: .top,
          endPoint: .bottom
        )

        RadialGradient(
          colors: [
            .clear,
            theme.paletteDarkOverlay(opacity: 0.54),
          ],
          center: .center,
          startRadius: 260,
          endRadius: 780
        )
      }
      .ignoresSafeArea()
    }
  }

  private var settings: DynamicParticleSettings {
    theme.particleSettings
  }

  private func drawLightSpeedStreaks(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    vanishingPoint: CGPoint
  ) {
    context.blendMode = .plusLighter

    let diagonal = hypot(size.width, size.height)
    let sizeScale = CGFloat(max(0.45, settings.size))
    let brightness = max(0.42, settings.brightness)
    let speed = max(0.25, settings.speed) * 0.25
    let count = min(260, max(90, Int(150 * settings.amount)))

    for index in 0..<count {
      let seedA = DynamicBackgroundMath.seededUnit(index: index, salt: 22.77)
      let seedB = DynamicBackgroundMath.seededUnit(index: index, salt: 65.21)
      let seedC = DynamicBackgroundMath.seededUnit(index: index, salt: 91.73)
      let angle =
        seedA * .pi * 2
        + sin(time * 0.02 + seedB * 10) * 0.055 * settings.dispersion
      let progress = DynamicBackgroundMath.unit(
        seedB
          + time * (0.26 + seedC * 0.72) * speed
      )
      let fadeIn = DynamicBackgroundMath.smoothstep(value: progress / 0.16)
      let fadeOut = 1 - DynamicBackgroundMath.smoothstep(value: (progress - 0.76) / 0.24)
      let lifecycleFade = max(0, fadeIn * fadeOut)
      let travel = CGFloat(pow(progress, 1.86))
      let direction = CGVector(
        dx: CGFloat(cos(angle)),
        dy: CGFloat(sin(angle))
      )
      let radius = diagonal * (0.016 + travel * 0.96)
      let length =
        min(
          diagonal * 0.24,
          CGFloat(1.4 + pow(progress, 2.7) * 250)
            * (0.48 + CGFloat(seedC) * 0.74)
            * sizeScale
        )
      let start = CGPoint(
        x: vanishingPoint.x + direction.dx * max(2, radius - length * 0.18),
        y: vanishingPoint.y + direction.dy * max(2, radius - length * 0.18)
      )
      let end = CGPoint(
        x: vanishingPoint.x + direction.dx * (radius + length * 0.82),
        y: vanishingPoint.y + direction.dy * (radius + length * 0.82)
      )
      let color =
        index.isMultiple(of: 9)
        ? theme.highlightColor(index: index, time: time)
        : theme.ribbonColor(index: index, time: time)
      let opacity =
        min(
          1,
          (0.08 + seedC * 0.26 + Double(travel) * 0.82)
            * lifecycleFade
            * brightness
        )
      let lineWidth =
        (0.18 + CGFloat(seedC) * 0.42 + travel * 2.25)
        * sizeScale

      drawWarpStreak(
        context: &context,
        start: start,
        end: end,
        color: color,
        opacity: opacity,
        lineWidth: lineWidth
      )
    }
  }

  private func drawBackgroundNebulas(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval
  ) {
    for index in 0..<7 {
      let seedA = DynamicBackgroundMath.seededUnit(index: index, salt: 13.19)
      let seedB = DynamicBackgroundMath.seededUnit(index: index, salt: 71.73)
      let seedC = DynamicBackgroundMath.seededUnit(index: index, salt: 41.11)
      let phase = time * (0.008 + seedA * 0.006)
      let center = CGPoint(
        x: size.width * CGFloat(0.08 + seedB * 0.84)
          + CGFloat(sin(phase + seedA * 8)) * size.width * 0.045,
        y: size.height * CGFloat(0.06 + seedC * 0.88)
          + CGFloat(cos(phase * 0.9 + seedB * 7)) * size.height * 0.04
      )
      let width = size.width * CGFloat(0.26 + seedA * 0.46)
      let height = size.height * CGFloat(0.11 + seedB * 0.24)
      let color =
        index.isMultiple(of: 2)
        ? theme.sharedColor(index: index, time: time)
        : theme.ribbonColor(index: index, time: time)
      let opacity = (0.012 + seedC * 0.016) * max(0.45, settings.brightness)

      context.drawLayer { nebula in
        nebula.blendMode = .plusLighter
        nebula.addFilter(.blur(radius: 54 + CGFloat(index % 3) * 18))
        nebula.fill(
          Path(ellipseIn: rect(center: center, width: width, height: height)),
          with: .color(color.opacity(opacity))
        )
      }
    }
  }

  private func drawStarParticles(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval
  ) {
    context.blendMode = .plusLighter

    let count = min(360, max(130, Int(190 * settings.amount)))
    let speed = max(0.12, settings.speed) * 0.18
    let sizeScale = CGFloat(max(0.45, settings.size))
    let brightness = max(0.36, settings.brightness)

    for index in 0..<count {
      let seedA = DynamicBackgroundMath.seededUnit(index: index, salt: 12.9898)
      let seedB = DynamicBackgroundMath.seededUnit(index: index, salt: 78.233)
      let seedC = DynamicBackgroundMath.seededUnit(index: index, salt: 37.719)
      let progress = DynamicBackgroundMath.unit(seedB + time * (0.006 + seedC * 0.008) * speed)
      let x =
        size.width * CGFloat(seedA)
        + CGFloat(sin(time * 0.015 + seedB * 9)) * 10 * CGFloat(settings.drift)
      let y =
        size.height * CGFloat(1 - progress)
        + CGFloat(cos(time * 0.012 + seedA * 8)) * 8 * CGFloat(settings.drift)
      let twinkle = pow(max(0, sin(time * (0.48 + seedC * 0.42) + seedA * 11)), 3)
      let side =
        CGFloat(index.isMultiple(of: 11) ? 1.8 : 0.75)
        * sizeScale
        * CGFloat(0.75 + twinkle * 0.6)
      let color =
        index.isMultiple(of: 13)
        ? theme.highlightColor(index: index, time: time)
        : theme.ribbonColor(index: index, time: time)
      let opacity =
        (index.isMultiple(of: 11) ? 0.26 : 0.12)
        + twinkle * 0.24

      context.fill(
        Path(
          CGRect(
            x: x - side / 2,
            y: y - side / 2,
            width: side,
            height: side
          )
        ),
        with: .color(color.opacity(opacity * brightness))
      )
    }
  }

  private func drawWarpStreak(
    context: inout GraphicsContext,
    start: CGPoint,
    end: CGPoint,
    color: Color,
    opacity: Double,
    lineWidth: CGFloat
  ) {
    var path = Path()
    path.move(to: start)
    path.addLine(to: end)

    context.stroke(
      path,
      with: .color(color.opacity(opacity)),
      style: StrokeStyle(
        lineWidth: lineWidth,
        lineCap: .round
      )
    )
  }

  private func vanishingPoint(size: CGSize, time: TimeInterval) -> CGPoint {
    CGPoint(
      x: size.width * (0.5 + CGFloat(sin(time * 0.036)) * 0.012),
      y: size.height * (0.5 + CGFloat(cos(time * 0.031)) * 0.01)
    )
  }
  private func rect(center: CGPoint, width: CGFloat, height: CGFloat) -> CGRect {
    CGRect(
      x: center.x - width / 2,
      y: center.y - height / 2,
      width: width,
      height: height
    )
  }

}

// MARK: - SpatialRetroBackground

struct SpatialRetroBackground: View {
  let theme: DynamicBackgroundTheme

  var body: some View {
    TimelineView(.animation(minimumInterval: 1 / 30)) { timeline in
      let time =
        timeline.date.timeIntervalSinceReferenceDate
        * theme.particleSettings.backgrounds.spatialRetroSpeed
      let primary = theme.sharedColor(index: 0, time: time)
      let secondary = theme.sharedColor(index: 1, time: time)
      let highlight = theme.ribbonColor(index: 2, time: time)
      let sharedGradient = theme.sharedGradientPoints(
        from: .topLeading,
        to: .bottomTrailing
      )

      ZStack {
        PaletteGradientField(
          colors: [
            theme.paletteBackgroundColor(primary, darkness: 1),
            theme.paletteBackgroundColor(primary, darkness: 0.78),
            theme.paletteBackgroundColor(secondary, darkness: 0.96),
            theme.paletteBackgroundColor(primary, darkness: 1),
          ],
          startPoint: sharedGradient.start,
          endPoint: sharedGradient.end,
          curvature: theme.sharedPaletteGradientCurvature
        )

        Circle()
          .fill(secondary.opacity(0.24))
          .frame(width: 720, height: 720)
          .blur(radius: 150)
          .offset(
            x: CGFloat(sin(time * 0.09)) * 120,
            y: CGFloat(cos(time * 0.07)) * 80
          )

        Canvas { context, size in
          drawPerspectiveGrid(
            context: &context,
            size: size,
            time: time,
            color: primary
          )
          drawPixelDust(
            context: &context,
            size: size,
            time: time,
            color: highlight
          )
          drawPolyhedra(
            context: &context,
            size: size,
            time: time
          )
        }

        LinearGradient(
          colors: [
            theme.paletteDarkOverlay(opacity: 0.08),
            .clear,
            theme.paletteDarkOverlay(opacity: 0.34),
          ],
          startPoint: .top,
          endPoint: .bottom
        )
      }
      .ignoresSafeArea()
    }
  }

  // Draws a moving perspective grid from the horizon to the bottom edge.
  private func drawPerspectiveGrid(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    color: Color
  ) {
    let horizon = size.height * 0.57
    let vanishingPoint = CGPoint(
      x: size.width * 0.5 + CGFloat(sin(time * 0.08)) * 22,
      y: horizon
    )

    for column in -10...10 {
      var line = Path()
      line.move(to: vanishingPoint)
      line.addLine(
        to: CGPoint(
          x: size.width * 0.5 + CGFloat(column) * size.width * 0.105,
          y: size.height
        )
      )
      context.stroke(
        line,
        with: .color(color.opacity(0.12)),
        lineWidth: 0.65
      )
    }

    for row in 0..<17 {
      let phase = (Double(row) / 17 + time * 0.055).truncatingRemainder(dividingBy: 1)
      let progress = CGFloat(phase)
      let depth = progress * progress
      let y = horizon + (size.height - horizon) * depth
      var line = Path()
      line.move(to: CGPoint(x: 0, y: y))
      line.addLine(to: CGPoint(x: size.width, y: y))
      context.stroke(
        line,
        with: .color(color.opacity(0.08 + Double(progress) * 0.22)),
        lineWidth: 0.6 + progress
      )
    }
  }

  // Draws small pixels that drift upward over the PS1-style grid.
  private func drawPixelDust(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    color: Color
  ) {
    for index in 0..<76 {
      let seed = Double(index) * 1.731
      let x = (sin(seed * 2.17) * 0.5 + 0.5) * size.width
      let progress = (Double(index) * 0.071 + time * (0.008 + Double(index % 4) * 0.002))
        .truncatingRemainder(dividingBy: 1)
      let y = (1 - progress) * size.height
      let side = CGFloat(index.isMultiple(of: 9) ? 2.2 : 1.1)
      context.fill(
        Path(
          CGRect(
            x: x - side / 2,
            y: y - side / 2,
            width: side,
            height: side
          )
        ),
        with: .color(color.opacity(index.isMultiple(of: 9) ? 0.46 : 0.2))
      )
    }
  }

  // Draws rotating faceted shapes and colors their facets from the ribbon palette.
  private func drawPolyhedra(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval
  ) {
    for shapeIndex in 0..<7 {
      let seed = Double(shapeIndex) * 1.43
      let center = CGPoint(
        x: size.width
          * CGFloat(0.12 + Double(shapeIndex % 4) * 0.25)
          + CGFloat(sin(time * 0.07 + seed)) * 28,
        y: size.height
          * CGFloat(0.18 + Double(shapeIndex / 4) * 0.36)
          + CGFloat(cos(time * 0.09 + seed)) * 34
      )
      let radius =
        min(size.width, size.height)
        * CGFloat(0.045 + Double(shapeIndex % 3) * 0.018)
      let vertexCount = 4 + shapeIndex % 3
      let rotation = time * (0.12 + Double(shapeIndex) * 0.008) + seed
      let innerPoint = CGPoint(
        x: center.x + CGFloat(cos(rotation * 0.7)) * radius * 0.17,
        y: center.y + CGFloat(sin(rotation * 0.9)) * radius * 0.12
      )

      var vertices: [CGPoint] = []
      for vertexIndex in 0..<vertexCount {
        let angle =
          rotation
          + Double(vertexIndex) * (.pi * 2 / Double(vertexCount))
        vertices.append(
          CGPoint(
            x: center.x + CGFloat(cos(angle)) * radius,
            y: center.y + CGFloat(sin(angle)) * radius * 0.72
          )
        )
      }

      for vertexIndex in vertices.indices {
        let nextIndex = (vertexIndex + 1) % vertices.count
        var facet = Path()
        facet.move(to: innerPoint)
        facet.addLine(to: vertices[vertexIndex])
        facet.addLine(to: vertices[nextIndex])
        facet.closeSubpath()

        let facetColor = theme.ribbonColor(
          index: shapeIndex + vertexIndex,
          time: time
        )
        context.fill(
          facet,
          with: .color(
            facetColor.opacity(
              0.12 + Double((vertexIndex + shapeIndex) % 3) * 0.07
            )
          )
        )
        context.stroke(
          facet,
          with: .color(facetColor.opacity(0.38)),
          lineWidth: 0.75
        )
      }
    }
  }

}

// MARK: - TowersOrbsBackground

struct TowersOrbsBackground: View {
  let theme: DynamicBackgroundTheme

  var body: some View {
    TimelineView(.animation(minimumInterval: 1 / 24)) { timeline in
      let time =
        timeline.date.timeIntervalSinceReferenceDate
        * theme.particleSettings.backgrounds.towersOrbsSpeed
      let primaryColor = theme.sharedColor(index: 0, time: time)
      let secondaryColor = theme.ribbonColor(index: 1, time: time)
      let darkColor = theme.sharedColor(index: 2, time: time)
      let sharedGradient = theme.sharedGradientPoints(
        from: .top,
        to: .bottom
      )

      ZStack {
        PaletteGradientField(
          colors: [
            theme.paletteBackgroundColor(darkColor, darkness: 0.98),
            theme.paletteBackgroundColor(darkColor, darkness: 0.72),
            theme.paletteBackgroundColor(primaryColor, darkness: 0.76),
            theme.paletteBackgroundColor(primaryColor, darkness: 1),
          ],
          startPoint: sharedGradient.start,
          endPoint: sharedGradient.end,
          curvature: theme.sharedPaletteGradientCurvature
        )

        Canvas { context, size in
          drawOrbitingOrbs(
            context: &context,
            size: size,
            time: time,
            drawsFront: false
          )
          drawTowers(context: &context, size: size, time: time)
          drawOrbitingOrbs(
            context: &context,
            size: size,
            time: time,
            drawsFront: true
          )
        }

        RadialGradient(
          colors: [secondaryColor.opacity(0.24), .clear],
          center: .topTrailing,
          startRadius: 10,
          endRadius: 360
        )
      }
      .ignoresSafeArea()
    }
  }

  // Draws layered PS2-style towers with ribbon-colored faces and edges.
  private func drawTowers(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval
  ) {
    for depth in 0..<5 {
      for column in 0..<8 {
        let index = depth * 8 + column
        let towerColor = theme.ribbonColor(index: index, time: time)
        let seed = Double(index) * 1.731
        let spacing = size.width / 7
        let x =
          CGFloat(column) * spacing
          + CGFloat(depth % 2) * spacing * 0.48
          - spacing * 0.22
          + CGFloat(sin(time * 0.035 + seed)) * 4
        let bottom = size.height * (0.62 + CGFloat(depth) * 0.085)
        let towerWidth = max(8, size.width * CGFloat(0.018 + Double(index % 4) * 0.005))
        let towerHeight =
          size.height
          * CGFloat(
            0.13 + (sin(seed * 1.21) * 0.5 + 0.5) * 0.38
          )
        let top = bottom - towerHeight
        let depthOffset = towerWidth * 0.48
        let depthRise = towerWidth * 0.28
        let frontRect = CGRect(
          x: x - towerWidth / 2,
          y: top,
          width: towerWidth,
          height: towerHeight
        )
        context.fill(
          Path(frontRect),
          with: .linearGradient(
            Gradient(colors: [
              towerColor.opacity(0.34),
              towerColor.opacity(0.08),
            ]),
            startPoint: CGPoint(x: x, y: top),
            endPoint: CGPoint(x: x, y: bottom)
          )
        )
        context.stroke(
          Path(frontRect),
          with: .color(towerColor.opacity(0.34)),
          lineWidth: 0.7
        )

        var side = Path()
        side.move(to: CGPoint(x: frontRect.maxX, y: frontRect.minY))
        side.addLine(
          to: CGPoint(
            x: frontRect.maxX + depthOffset,
            y: frontRect.minY - depthRise
          )
        )
        side.addLine(
          to: CGPoint(
            x: frontRect.maxX + depthOffset,
            y: frontRect.maxY - depthRise
          )
        )
        side.addLine(to: CGPoint(x: frontRect.maxX, y: frontRect.maxY))
        side.closeSubpath()
        context.fill(side, with: .color(towerColor.opacity(0.1)))
        context.stroke(
          side,
          with: .color(towerColor.opacity(0.24)),
          lineWidth: 0.6
        )

        var topFace = Path()
        topFace.move(to: CGPoint(x: frontRect.minX, y: frontRect.minY))
        topFace.addLine(to: CGPoint(x: frontRect.maxX, y: frontRect.minY))
        topFace.addLine(
          to: CGPoint(
            x: frontRect.maxX + depthOffset,
            y: frontRect.minY - depthRise
          )
        )
        topFace.addLine(
          to: CGPoint(
            x: frontRect.minX + depthOffset,
            y: frontRect.minY - depthRise
          )
        )
        topFace.closeSubpath()
        context.fill(topFace, with: .color(towerColor.opacity(0.42)))
        context.stroke(
          topFace,
          with: .color(.white.opacity(0.16)),
          lineWidth: 0.5
        )
      }
    }
  }

  // Draws orbital trails in back and front passes so each orb appears to wrap around.
  private func drawOrbitingOrbs(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    drawsFront: Bool
  ) {
    for orbIndex in 0..<4 {
      let orbColor = theme.ribbonColor(index: orbIndex, time: time)
      let configuration = orbitConfiguration(index: orbIndex, size: size)
      let currentMoment = time + configuration.startOffset

      for tailIndex in 0..<13 {
        let firstMoment = currentMoment - Double(tailIndex) * 0.12
        let secondMoment = currentMoment - Double(tailIndex + 1) * 0.12
        let middleDepth = orbitDepth(
          moment: (firstMoment + secondMoment) / 2,
          configuration: configuration
        )
        guard (middleDepth >= 0) == drawsFront else { continue }

        let first = orbitPoint(
          moment: firstMoment,
          configuration: configuration
        )
        let second = orbitPoint(
          moment: secondMoment,
          configuration: configuration
        )
        let delta = secondMoment - firstMoment
        let firstTangent = orbitTangent(
          moment: firstMoment,
          configuration: configuration
        )
        let secondTangent = orbitTangent(
          moment: secondMoment,
          configuration: configuration
        )
        let control1 = CGPoint(
          x: first.x + firstTangent.width * delta / 3,
          y: first.y + firstTangent.height * delta / 3
        )
        let control2 = CGPoint(
          x: second.x - secondTangent.width * delta / 3,
          y: second.y - secondTangent.height * delta / 3
        )
        var tail = Path()
        tail.move(to: first)
        tail.addCurve(
          to: second,
          control1: control1,
          control2: control2
        )

        let fade = 1 - Double(tailIndex) / 13
        context.stroke(
          tail,
          with: .color(orbColor.opacity(fade * (drawsFront ? 0.72 : 0.2))),
          style: StrokeStyle(
            lineWidth: CGFloat(0.6 + fade * 2.1),
            lineCap: .round
          )
        )
      }

      let currentDepth = orbitDepth(
        moment: currentMoment,
        configuration: configuration
      )
      guard (currentDepth >= 0) == drawsFront else { continue }
      let point = orbitPoint(
        moment: currentMoment,
        configuration: configuration
      )
      let depthScale = CGFloat(0.72 + (currentDepth + 1) * 0.2)
      let diameter = CGFloat(7 + orbIndex) * depthScale
      let rect = CGRect(
        x: point.x - diameter / 2,
        y: point.y - diameter / 2,
        width: diameter,
        height: diameter
      )

      context.drawLayer { glow in
        glow.addFilter(.shadow(color: orbColor.opacity(0.9), radius: 8))
        glow.fill(
          Path(ellipseIn: rect),
          with: .color(
            drawsFront ? .white : orbColor.opacity(0.45)
          )
        )
      }
    }
  }

  // Creates seeded start positions and non-circular movement values for each orb.
  private func orbitConfiguration(
    index: Int,
    size: CGSize
  ) -> TowerOrbitConfiguration {
    let seedA = DynamicBackgroundMath.seededUnit(index: index, salt: 17.129)
    let seedB = DynamicBackgroundMath.seededUnit(index: index, salt: 31.415)
    let seedC = DynamicBackgroundMath.seededUnit(index: index, salt: 57.721)
    let seedD = DynamicBackgroundMath.seededUnit(index: index, salt: 83.113)
    let seedE = DynamicBackgroundMath.seededUnit(index: index, salt: 109.37)
    let seedF = DynamicBackgroundMath.seededUnit(index: index, salt: 139.81)

    return TowerOrbitConfiguration(
      center: CGPoint(
        x: size.width * CGFloat(0.18 + seedA * 0.64),
        y: size.height * CGFloat(0.32 + seedB * 0.36)
      ),
      radiusX: size.width * CGFloat(0.1 + seedC * 0.17),
      radiusY: size.height * CGFloat(0.05 + seedD * 0.09),
      wobbleX: size.width * CGFloat(0.025 + seedE * 0.065),
      wobbleY: size.height * CGFloat(0.02 + seedF * 0.06),
      speedX: 0.16 + seedB * 0.18,
      speedY: 0.2 + seedC * 0.2,
      wobbleSpeed: 0.34 + seedD * 0.32,
      phaseX: seedE * .pi * 2,
      phaseY: seedF * .pi * 2,
      wobblePhase: seedA * .pi * 2,
      depthSpeed: 0.22 + seedF * 0.18,
      depthPhase: seedC * .pi * 2,
      startOffset: seedD * 11
    )
  }

  // Returns the current point on a layered, non-circular orb path.
  private func orbitPoint(
    moment: Double,
    configuration: TowerOrbitConfiguration
  ) -> CGPoint {
    CGPoint(
      x: configuration.center.x
        + CGFloat(cos(moment * configuration.speedX + configuration.phaseX))
        * configuration.radiusX
        + CGFloat(sin(moment * configuration.wobbleSpeed + configuration.wobblePhase))
        * configuration.wobbleX
        + CGFloat(cos(moment * 0.11 + configuration.phaseY))
        * configuration.radiusX
        * 0.18,
      y: configuration.center.y
        + CGFloat(sin(moment * configuration.speedY + configuration.phaseY))
        * configuration.radiusY
        + CGFloat(cos(moment * configuration.wobbleSpeed * 0.82 + configuration.wobblePhase))
        * configuration.wobbleY
        + CGFloat(sin(moment * 0.13 + configuration.phaseX))
        * configuration.radiusY
        * 0.22
    )
  }

  // Samples nearby points to curve the orb trail along the seeded path.
  private func orbitTangent(
    moment: Double,
    configuration: TowerOrbitConfiguration
  ) -> CGSize {
    let delta = 0.01
    let first = orbitPoint(
      moment: moment - delta,
      configuration: configuration
    )
    let second = orbitPoint(
      moment: moment + delta,
      configuration: configuration
    )

    return CGSize(
      width: (second.x - first.x) / CGFloat(delta * 2),
      height: (second.y - first.y) / CGFloat(delta * 2)
    )
  }

  // Calculates whether an orb is currently behind or in front of the towers.
  private func orbitDepth(
    moment: Double,
    configuration: TowerOrbitConfiguration
  ) -> Double {
    sin(moment * configuration.depthSpeed + configuration.depthPhase)
  }
}

private struct TowerOrbitConfiguration {
  let center: CGPoint
  let radiusX: CGFloat
  let radiusY: CGFloat
  let wobbleX: CGFloat
  let wobbleY: CGFloat
  let speedX: Double
  let speedY: Double
  let wobbleSpeed: Double
  let phaseX: Double
  let phaseY: Double
  let wobblePhase: Double
  let depthSpeed: Double
  let depthPhase: Double
  let startOffset: Double
}

// MARK: - PlayStation2MenuScene

enum PlayStation2MenuDepthPass {
  case far
  case middle
  case near

  func contains(_ depth: Double) -> Bool {
    switch self {
    case .far:
      return depth < -0.25
    case .middle:
      return depth >= -0.25 && depth < 0.45
    case .near:
      return depth >= 0.45
    }
  }

  func visibility(at depth: Double) -> Double {
    let feather = 0.16
    let middleEntry = DynamicBackgroundMath.smoothstep(
      from: -0.25 - feather,
      to: -0.25 + feather,
      value: depth
    )
    let nearEntry = DynamicBackgroundMath.smoothstep(
      from: 0.45 - feather,
      to: 0.45 + feather,
      value: depth
    )

    switch self {
    case .far:
      return 1 - middleEntry
    case .middle:
      return middleEntry * (1 - nearEntry)
    case .near:
      return middleEntry * nearEntry
    }
  }
}

struct PlayStation2MenuCamera {
  let size: CGSize
  let horizontalExtent: Double
  let verticalExtent: Double

  private let yawSine: Double
  private let yawCosine: Double
  private let pitchSine: Double
  private let pitchCosine: Double
  private let rollSine: Double
  private let rollCosine: Double
  private let viewportScale: Double
  private let panX: Double
  private let panY: Double

  init(
    size: CGSize,
    time: TimeInterval,
    yawSpeed: Double,
    rollSpeed: Double,
    orbitDegrees: Double
  ) {
    self.size = size

    let minimumDimension = max(1, min(size.width, size.height))
    horizontalExtent = Double(size.width / minimumDimension)
    verticalExtent = Double(size.height / minimumDimension)
    viewportScale = Double(minimumDimension * 0.5)

    let orbitLimit = orbitDegrees * Double.pi / 180
    let yawMotion =
      sin(time * 0.024 * yawSpeed) * 0.055
      + sin(time * 0.008 * yawSpeed) * 0.015
    let yaw = yawMotion / 0.07 * orbitLimit
    let pitch = sin(time * 0.019 + 1.2) * 0.035
    let roll = sin(time * 0.014 * rollSpeed) * 0.012

    yawSine = sin(yaw)
    yawCosine = cos(yaw)
    pitchSine = sin(pitch)
    pitchCosine = cos(pitch)
    rollSine = sin(roll)
    rollCosine = cos(roll)
    panX =
      sin(time * 0.013) * Double(size.width) * 0.024
      + sin(time * 0.005 + 1.3) * Double(size.width) * 0.008
    panY =
      cos(time * 0.011) * Double(size.height) * 0.018
      + sin(time * 0.004 + 0.7) * Double(size.height) * 0.007
  }

  func project(_ world: PlayStation2MenuVector3) -> PlayStation2MenuProjection {
    let yawX = world.x * yawCosine + world.z * yawSine
    let yawZ = -world.x * yawSine + world.z * yawCosine
    let pitchY = world.y * pitchCosine - yawZ * pitchSine
    let pitchZ = world.y * pitchSine + yawZ * pitchCosine
    let transformedX = yawX * rollCosine - pitchY * rollSine
    let transformedY = yawX * rollSine + pitchY * rollCosine
    let perspective = 1 / max(0.68, 1 - pitchZ * 0.17)

    return PlayStation2MenuProjection(
      point: CGPoint(
        x: size.width * 0.5
          + CGFloat(transformedX * viewportScale * perspective + panX),
        y: size.height * 0.52
          + CGFloat(transformedY * viewportScale * perspective + panY)
          - CGFloat(pitchZ) * size.height * 0.018
      ),
      depth: pitchZ,
      scale: CGFloat(perspective)
    )
  }
}

struct PlayStation2MenuVector3 {
  let x: Double
  let y: Double
  let z: Double

  static func + (
    lhs: PlayStation2MenuVector3,
    rhs: PlayStation2MenuVector3
  ) -> PlayStation2MenuVector3 {
    PlayStation2MenuVector3(
      x: lhs.x + rhs.x,
      y: lhs.y + rhs.y,
      z: lhs.z + rhs.z
    )
  }

  static func * (
    vector: PlayStation2MenuVector3,
    scalar: Double
  ) -> PlayStation2MenuVector3 {
    PlayStation2MenuVector3(
      x: vector.x * scalar,
      y: vector.y * scalar,
      z: vector.z * scalar
    )
  }

  func rotatedX(_ angle: Double) -> PlayStation2MenuVector3 {
    PlayStation2MenuVector3(
      x: x,
      y: y * cos(angle) - z * sin(angle),
      z: y * sin(angle) + z * cos(angle)
    )
  }

  func rotatedY(_ angle: Double) -> PlayStation2MenuVector3 {
    PlayStation2MenuVector3(
      x: x * cos(angle) + z * sin(angle),
      y: y,
      z: -x * sin(angle) + z * cos(angle)
    )
  }

  func rotatedZ(_ angle: Double) -> PlayStation2MenuVector3 {
    PlayStation2MenuVector3(
      x: x * cos(angle) - y * sin(angle),
      y: x * sin(angle) + y * cos(angle),
      z: z
    )
  }
}

struct PlayStation2MenuRotation {
  let x: Double
  let y: Double
  let z: Double
}

struct PlayStation2MenuProjection {
  let point: CGPoint
  let depth: Double
  let scale: CGFloat
}

struct PlayStation2MenuTowerConfiguration {
  let center: PlayStation2MenuVector3
  let size: PlayStation2MenuVector3
  let rotation: PlayStation2MenuRotation
  let pulseSpeed: Double
  let pulseAmount: Double
  let phase: Double
  let isInner: Bool
}

struct PlayStation2MenuCrystalConfiguration {
  let center: PlayStation2MenuVector3
  let side: Double
  let phase: Double
  let rotationSpeed: Double
  let tintIndex: Int
}

struct PlayStation2MenuCloudElement {
  let rect: CGRect
  let glowRect: CGRect
  let color: Color
  let cloudOpacity: Double
  let glowOpacity: Double
}

struct PlayStation2MenuOrbConfiguration {
  let center: PlayStation2MenuVector3
  let radiusX: Double
  let radiusY: Double
  let bend: Double
  let speed: Double
  let phase: Double
  let depthPhase: Double
}

enum PlayStation2MenuGeometry {
  static let cubeFaces: [[Int]] = [
    [0, 1, 2, 3],
    [4, 7, 6, 5],
    [0, 4, 5, 1],
    [1, 5, 6, 2],
    [2, 6, 7, 3],
    [3, 7, 4, 0],
  ]

  static let crystalCrossEdges: [(Int, Int)] = [
    (0, 6),
    (1, 7),
    (2, 4),
    (3, 5),
  ]
}

// MARK: - PlayStation2MenuBackground

struct PlayStation2MenuBackground: View {
  let theme: DynamicBackgroundTheme
  @Environment(\.menuBackgroundSessionStart) private var cameraStartDate

  var body: some View {
    let framesPerSecond = max(
      1,
      theme.particleSettings.backgrounds.playStation2MenuFramesPerSecond
    )

    TimelineView(.animation(minimumInterval: 1 / framesPerSecond)) { timeline in
      let motionSettings = theme.particleSettings.backgrounds
      let time =
        timeline.date.timeIntervalSinceReferenceDate
        * motionSettings.playStation2MenuSceneSpeed
      let cameraTime =
        max(0, timeline.date.timeIntervalSince(cameraStartDate))
        * motionSettings.playStation2MenuSceneSpeed
      let deepColor = theme.sharedColor(index: 0, time: time)
      let cloudColor = theme.sharedColor(index: 1, time: time)
      let reflectedColor = theme.ribbonColor(index: 2, time: time)
      let sharedGradient = theme.sharedGradientPoints(
        from: .topLeading,
        to: .bottomTrailing
      )

      ZStack {
        theme.paletteBackgroundColor(deepColor, darkness: 1)

        PaletteGradientField(
          colors: [
            theme.paletteBackgroundColor(deepColor, darkness: 1),
            theme.paletteBackgroundColor(deepColor, darkness: 0.98),
            theme.paletteBackgroundColor(deepColor, darkness: 0.89),
            theme.paletteBackgroundColor(deepColor, darkness: 1),
          ],
          startPoint: sharedGradient.start,
          endPoint: sharedGradient.end,
          curvature: theme.sharedPaletteGradientCurvature
        )

        RadialGradient(
          colors: [
            cloudColor.opacity(0.2),
            reflectedColor.opacity(0.065),
            deepColor.opacity(0.035),
            .clear,
          ],
          center: UnitPoint(
            x: 0.53 + CGFloat(sin(time * 0.019)) * 0.035,
            y: 0.58 + CGFloat(cos(time * 0.016)) * 0.045
          ),
          startRadius: 18,
          endRadius: 700
        )

        Canvas(
          opaque: false,
          colorMode: .nonLinear,
          rendersAsynchronously: true
        ) { context, size in
          guard size.width > 1, size.height > 1 else { return }

          let camera = PlayStation2MenuCamera(
            size: size,
            time: cameraTime,
            yawSpeed: motionSettings.playStation2MenuYawSpeed,
            rollSpeed: motionSettings.playStation2MenuRollSpeed,
            orbitDegrees: motionSettings.playStation2MenuOrbitDegrees
          )
          let towers = towerConfigurations(camera: camera)
          let crystals = crystalConfigurations(camera: camera)

          drawFog(
            context: &context,
            camera: camera,
            time: time,
            foreground: false
          )

          drawOrbTrails(
            context: &context,
            camera: camera,
            time: time,
            pass: .far
          )
          drawTowers(
            context: &context,
            camera: camera,
            time: time,
            pass: .far,
            configurations: towers
          )
          drawCrystals(
            context: &context,
            camera: camera,
            time: time,
            pass: .far,
            configurations: crystals
          )

          drawTowers(
            context: &context,
            camera: camera,
            time: time,
            pass: .middle,
            configurations: towers
          )
          drawOrbTrails(
            context: &context,
            camera: camera,
            time: time,
            pass: .middle
          )
          drawCrystals(
            context: &context,
            camera: camera,
            time: time,
            pass: .middle,
            configurations: crystals
          )

          drawTowers(
            context: &context,
            camera: camera,
            time: time,
            pass: .near,
            configurations: towers
          )
          drawTowerBaseClouds(
            context: &context,
            camera: camera,
            time: time,
            configurations: towers
          )
          drawCrystals(
            context: &context,
            camera: camera,
            time: time,
            pass: .near,
            configurations: crystals
          )
          drawOrbTrails(
            context: &context,
            camera: camera,
            time: time,
            pass: .near
          )
        }

        LinearGradient(
          colors: [
            theme.paletteDarkOverlay(opacity: 0.5),
            .clear,
            theme.paletteDarkOverlay(opacity: 0.24),
          ],
          startPoint: .top,
          endPoint: .bottom
        )

        RadialGradient(
          colors: [
            .clear,
            theme.paletteDarkOverlay(opacity: 0.76),
          ],
          center: .center,
          startRadius: 230,
          endRadius: 840
        )
      }
      .ignoresSafeArea()
      .accessibilityHidden(true)
    }
  }
}

// MARK: - PlayStation2MenuRenderer

extension PlayStation2MenuBackground {
  func drawFog(
    context: inout GraphicsContext,
    camera: PlayStation2MenuCamera,
    time: TimeInterval,
    foreground: Bool
  ) {
    let count = foreground ? 4 : 7
    context.drawLayer { fog in
      fog.blendMode = .plusLighter
      fog.addFilter(.blur(radius: foreground ? 72 : 62))

      for index in 0..<count {
        let seedA = DynamicBackgroundMath.seededUnit(index: index, salt: foreground ? 31.73 : 13.19)
        let seedB = DynamicBackgroundMath.seededUnit(index: index, salt: foreground ? 83.41 : 57.13)
        let seedC = DynamicBackgroundMath.seededUnit(
          index: index, salt: foreground ? 119.17 : 97.61)
        let depth =
          foreground
          ? 0.48 + seedC * 0.52
          : -1.08 + seedC * 0.9
        let worldCenter = PlayStation2MenuVector3(
          x: (-0.92 + seedA * 1.84) * camera.horizontalExtent,
          y: (-0.78 + seedB * 1.56) * camera.verticalExtent,
          z: depth
        )
        let projected = camera.project(worldCenter)
        let driftX = CGFloat(sin(time * (0.009 + seedA * 0.008) + seedB * 9)) * 34
        let driftY = CGFloat(cos(time * (0.008 + seedC * 0.007) + seedA * 11)) * 26
        let width = camera.size.width * CGFloat(0.2 + seedB * 0.31) * projected.scale
        let height = camera.size.height * CGFloat(0.08 + seedC * 0.15) * projected.scale
        let rect = CGRect(
          x: projected.point.x + driftX - width / 2,
          y: projected.point.y + driftY - height / 2,
          width: width,
          height: height
        )
        let color =
          index.isMultiple(of: 3)
          ? theme.sharedColor(index: index + 1, time: time)
          : theme.ribbonColor(index: index + 2, time: time)
        let opacity =
          foreground
          ? 0.018 + seedA * 0.012
          : 0.018 + seedA * 0.022

        fog.fill(
          Path(ellipseIn: rect),
          with: .color(color.opacity(opacity))
        )
      }
    }
  }
}

// MARK: - PlayStation2MenuTowerRenderer

extension PlayStation2MenuBackground {
  func drawTowers(
    context: inout GraphicsContext,
    camera: PlayStation2MenuCamera,
    time: TimeInterval,
    pass: PlayStation2MenuDepthPass,
    configurations: [PlayStation2MenuTowerConfiguration]
  ) {
    let visibleConfigurations =
      configurations
      .enumerated()
      .filter { pass.contains($0.element.center.z) }
      .sorted { $0.element.center.z < $1.element.center.z }

    for (index, configuration) in visibleConfigurations {
      let pulse =
        1
        + sin(time * configuration.pulseSpeed + configuration.phase)
        * configuration.pulseAmount
      drawTower(
        context: &context,
        camera: camera,
        configuration: configuration,
        growth: pulse,
        time: time,
        index: index,
        pass: pass
      )
    }
  }

  func drawTower(
    context: inout GraphicsContext,
    camera: PlayStation2MenuCamera,
    configuration: PlayStation2MenuTowerConfiguration,
    growth: Double,
    time: TimeInterval,
    index: Int,
    pass: PlayStation2MenuDepthPass
  ) {
    let baseCenter = towerBaseCenter(
      configuration: configuration,
      growth: growth
    )
    let top = towerFaceCorners(
      center: configuration.center,
      size: configuration.size,
      scale: 1,
      rotation: configuration.rotation,
      camera: camera
    )
    let base = towerFaceCorners(
      center: baseCenter,
      size: configuration.size,
      scale: configuration.isInner ? 0.7 : 0.56,
      rotation: configuration.rotation,
      camera: camera
    )
    let sideFaces = [
      [base[0], base[1], top[1], top[0]],
      [base[1], base[2], top[2], top[1]],
      [base[2], base[3], top[3], top[2]],
      [base[3], base[0], top[0], top[3]],
    ].enumerated().sorted { lhs, rhs in
      averagedDepth(lhs.element) < averagedDepth(rhs.element)
    }
    let tint =
      index.isMultiple(of: 7)
      ? theme.ribbonColor(index: index, time: time)
      : theme.sharedColor(index: index, time: time)
    let colorsTowers =
      theme.particleSettings.backgrounds.playStation2MenuColorsTowers
    let materialOpacity: Double

    switch pass {
    case .far:
      materialOpacity = 0.22
    case .middle:
      materialOpacity = 0.42
    case .near:
      materialOpacity = 0.62
    }

    let faceLight: [Double] = [0.28, 0.68, 0.42, 0.78]

    for (faceIndex, face) in sideFaces {
      let path = polygonPath(face.map { $0.point })
      let light = faceLight[faceIndex]
      let baseEdgeCenter = averagedPoint([face[0].point, face[1].point])
      let topEdgeCenter = averagedPoint([face[2].point, face[3].point])
      let depthBase: (red: Double, green: Double, blue: Double)

      switch pass {
      case .far:
        depthBase = (0.14, 0.18, 0.26)
      case .middle:
        depthBase = (0.24, 0.3, 0.4)
      case .near:
        depthBase = (0.38, 0.46, 0.58)
      }

      let neutral = Color(
        red: depthBase.red + light * 0.12,
        green: depthBase.green + light * 0.11,
        blue: depthBase.blue + light * 0.1
      )
      let towerSurface = colorsTowers ? tint : neutral

      context.fill(
        path,
        with: .linearGradient(
          Gradient(stops: [
            .init(color: .clear, location: 0),
            .init(
              color: tint.opacity(
                materialOpacity * (colorsTowers ? 0.18 : 0.025)
              ),
              location: 0.16
            ),
            .init(
              color: towerSurface.opacity(
                materialOpacity * (colorsTowers ? 0.38 : 0.2)
              ),
              location: 0.34
            ),
            .init(
              color: towerSurface.opacity(materialOpacity * (0.7 + light * 0.12)),
              location: 0.88
            ),
            .init(
              color: .white.opacity(materialOpacity * light * 0.24),
              location: 1
            ),
          ]),
          startPoint: baseEdgeCenter,
          endPoint: topEdgeCenter
        )
      )

      context.stroke(
        path,
        with: .linearGradient(
          Gradient(stops: [
            .init(color: .clear, location: 0),
            .init(
              color: .white.opacity(materialOpacity * 0.035),
              location: 0.34
            ),
            .init(
              color: .white.opacity(materialOpacity * (0.08 + light * 0.05)),
              location: 1
            ),
          ]),
          startPoint: baseEdgeCenter,
          endPoint: topEdgeCenter
        ),
        lineWidth: pass == .near ? 0.7 : 0.45
      )
    }

    let topPath = polygonPath(top.map { $0.point })
    let topBounds = topPath.boundingRect
    let topColor: Color

    switch pass {
    case .far:
      topColor = Color(red: 0.3, green: 0.36, blue: 0.47)
    case .middle:
      topColor = Color(red: 0.46, green: 0.54, blue: 0.65)
    case .near:
      topColor = Color(red: 0.62, green: 0.7, blue: 0.8)
    }

    context.fill(
      topPath,
      with: .linearGradient(
        Gradient(colors: [
          .white.opacity(materialOpacity * 0.24),
          (colorsTowers ? tint : topColor).opacity(materialOpacity * 0.76),
          tint.opacity(materialOpacity * (colorsTowers ? 0.48 : 0.14)),
          .black.opacity(materialOpacity * 0.22),
        ]),
        startPoint: topBounds.origin,
        endPoint: CGPoint(x: topBounds.maxX, y: topBounds.maxY)
      )
    )
    context.stroke(
      topPath,
      with: .color(.white.opacity(materialOpacity * 0.13)),
      lineWidth: pass == .near ? 0.75 : 0.5
    )
  }

  func towerBaseCenter(
    configuration: PlayStation2MenuTowerConfiguration,
    growth: Double
  ) -> PlayStation2MenuVector3 {
    let radialDistance = max(
      0.001,
      hypot(configuration.center.x, configuration.center.y)
    )
    let inward = PlayStation2MenuVector3(
      x: -configuration.center.x / radialDistance,
      y: -configuration.center.y / radialDistance,
      z: 0
    )
    let maximumShaftLength = radialDistance * (configuration.isInner ? 0.22 : 0.68)
    let shaftLength = min(configuration.size.z * growth, maximumShaftLength)

    return PlayStation2MenuVector3(
      x: configuration.center.x + inward.x * shaftLength,
      y: configuration.center.y + inward.y * shaftLength,
      z: configuration.center.z - 0.1 - shaftLength * 0.18
    )
  }

  func towerFaceCorners(
    center: PlayStation2MenuVector3,
    size: PlayStation2MenuVector3,
    scale: Double,
    rotation: PlayStation2MenuRotation,
    camera: PlayStation2MenuCamera
  ) -> [PlayStation2MenuProjection] {
    let halfWidth = size.x * scale * 0.5
    let halfHeight = size.y * scale * 0.5

    return [
      PlayStation2MenuVector3(x: -halfWidth, y: -halfHeight, z: 0),
      PlayStation2MenuVector3(x: halfWidth, y: -halfHeight, z: 0),
      PlayStation2MenuVector3(x: halfWidth, y: halfHeight, z: 0),
      PlayStation2MenuVector3(x: -halfWidth, y: halfHeight, z: 0),
    ].map { local in
      camera.project(center + local.rotatedZ(rotation.z))
    }
  }

  func averagedDepth(_ projections: [PlayStation2MenuProjection]) -> Double {
    projections.reduce(0) { $0 + $1.depth } / Double(projections.count)
  }

  func drawTowerBaseClouds(
    context: inout GraphicsContext,
    camera: PlayStation2MenuCamera,
    time: TimeInterval,
    configurations: [PlayStation2MenuTowerConfiguration]
  ) {
    let bankCenter = camera.project(
      PlayStation2MenuVector3(
        x: -0.06 * camera.horizontalExtent,
        y: 0.08 * camera.verticalExtent,
        z: -0.08
      )
    ).point
    let bankRect = CGRect(
      x: bankCenter.x - camera.size.width * 0.31,
      y: bankCenter.y - camera.size.height * 0.15,
      width: camera.size.width * 0.62,
      height: camera.size.height * 0.3
    )

    context.drawLayer { bank in
      bank.addFilter(.blur(radius: 68))
      bank.fill(
        Path(ellipseIn: bankRect),
        with: .color(Color(red: 0.08, green: 0.12, blue: 0.2).opacity(0.3))
      )
    }
    context.drawLayer { bankGlow in
      bankGlow.blendMode = .plusLighter
      bankGlow.addFilter(.blur(radius: 84))
      bankGlow.fill(
        Path(ellipseIn: bankRect.insetBy(dx: bankRect.width * 0.08, dy: 0)),
        with: .color(theme.sharedColor(index: 1, time: time).opacity(0.075))
      )
    }

    let outerConfigurations =
      configurations
      .enumerated()
      .filter { !$0.element.isInner }
    let minimumDimension = min(camera.size.width, camera.size.height)

    let cloudElements = outerConfigurations.map { entry in
      let (index, configuration) = entry
      let pulse =
        1
        + sin(time * configuration.pulseSpeed + configuration.phase)
        * configuration.pulseAmount
      let projected = camera.project(
        towerBaseCenter(configuration: configuration, growth: pulse)
      )
      let seedA = DynamicBackgroundMath.seededUnit(index: index, salt: 37.11)
      let seedB = DynamicBackgroundMath.seededUnit(index: index, salt: 79.63)
      let drift = CGPoint(
        x: CGFloat(sin(time * 0.012 + Double(index) * 1.7)) * 12,
        y: CGFloat(cos(time * 0.01 + Double(index) * 1.3)) * 9
      )
      let width = minimumDimension * CGFloat(0.21 + seedA * 0.09)
      let height = minimumDimension * CGFloat(0.09 + seedB * 0.055)
      let rect = CGRect(
        x: projected.point.x + drift.x - width / 2,
        y: projected.point.y + drift.y - height / 2,
        width: width,
        height: height
      )
      let color =
        index.isMultiple(of: 3)
        ? theme.sharedColor(index: index + 1, time: time)
        : theme.ribbonColor(index: index + 2, time: time)

      return PlayStation2MenuCloudElement(
        rect: rect,
        glowRect: rect.insetBy(dx: width * 0.1, dy: height * 0.08),
        color: color,
        cloudOpacity: 0.28 + seedA * 0.12,
        glowOpacity: 0.08 + seedB * 0.045
      )
    }

    context.drawLayer { cloud in
      cloud.addFilter(.blur(radius: 34))
      for element in cloudElements {
        cloud.fill(
          Path(ellipseIn: element.rect),
          with: .color(
            Color(red: 0.16, green: 0.21, blue: 0.3)
              .opacity(element.cloudOpacity)
          )
        )
      }
    }
    context.drawLayer { glow in
      glow.blendMode = .plusLighter
      glow.addFilter(.blur(radius: 47))
      for element in cloudElements {
        glow.fill(
          Path(ellipseIn: element.glowRect),
          with: .color(element.color.opacity(element.glowOpacity))
        )
      }
    }
  }
}

// MARK: - PlayStation2MenuCrystalRenderer

extension PlayStation2MenuBackground {
  func drawCrystals(
    context: inout GraphicsContext,
    camera: PlayStation2MenuCamera,
    time: TimeInterval,
    pass: PlayStation2MenuDepthPass,
    configurations: [PlayStation2MenuCrystalConfiguration]
  ) {
    for (index, configuration) in configurations.enumerated() {
      guard pass.contains(configuration.center.z) else { continue }

      drawCrystal(
        context: &context,
        camera: camera,
        configuration: configuration,
        time: time,
        index: index,
        pass: pass
      )
    }
  }

  func drawCrystal(
    context: inout GraphicsContext,
    camera: PlayStation2MenuCamera,
    configuration: PlayStation2MenuCrystalConfiguration,
    time: TimeInterval,
    index: Int,
    pass: PlayStation2MenuDepthPass
  ) {
    let rotation = PlayStation2MenuRotation(
      x: configuration.phase + time * configuration.rotationSpeed * 0.68,
      y: configuration.phase * 0.73 + time * configuration.rotationSpeed,
      z: configuration.phase * 0.41 + time * configuration.rotationSpeed * 0.38
    )
    let half = PlayStation2MenuVector3(
      x: configuration.side / 2,
      y: configuration.side / 2,
      z: configuration.side / 2
    )
    let projected = cuboidVertices(
      center: configuration.center,
      halfSize: half,
      rotation: rotation,
      camera: camera
    )
    let faces = PlayStation2MenuGeometry.cubeFaces.enumerated().sorted { lhs, rhs in
      averageDepth(lhs.element, vertices: projected)
        < averageDepth(rhs.element, vertices: projected)
    }
    let shared = theme.sharedColor(index: configuration.tintIndex, time: time)
    let accent = theme.ribbonColor(index: configuration.tintIndex + 1, time: time)
    let opacity: Double

    switch pass {
    case .far:
      opacity = 0.1
    case .middle:
      opacity = 0.18
    case .near:
      opacity = 0.27
    }

    for (sortedIndex, faceEntry) in faces.enumerated() {
      let faceIndex = faceEntry.offset
      let face = faceEntry.element
      let points = face.map { projected[$0].point }
      let path = polygonPath(points)
      let bounds = path.boundingRect
      let depth = averageDepth(face, vertices: projected)
      let sheen = 0.45 + max(-0.25, min(0.45, depth))

      context.fill(
        path,
        with: .linearGradient(
          Gradient(colors: [
            .white.opacity(opacity * (0.24 + sheen * 0.2)),
            shared.opacity(opacity * (0.42 + sheen * 0.18)),
            accent.opacity(opacity * 0.5),
            .black.opacity(opacity * 0.28),
          ]),
          startPoint: CGPoint(x: bounds.minX, y: bounds.minY),
          endPoint: CGPoint(x: bounds.maxX, y: bounds.maxY)
        )
      )

      let reflectiveFaceCount = pass == .far ? 2 : 3
      if sortedIndex >= faces.count - reflectiveFaceCount {
        let reflectionIdentity =
          index * PlayStation2MenuGeometry.cubeFaces.count + faceIndex
        drawCrystalCloudReflection(
          context: &context,
          clippingPath: path,
          bounds: bounds,
          time: time,
          index: reflectionIdentity,
          opacity: opacity
        )
        drawCrystalOrbReflections(
          context: &context,
          clippingPath: path,
          bounds: bounds,
          time: time,
          index: reflectionIdentity,
          reflectionCount: pass == .near ? 2 : 1,
          opacity: opacity
        )
      }

      if let first = points.first, points.count > 2 {
        var facet = Path()
        facet.move(to: first)
        facet.addLine(to: averagedPoint(points))
        facet.addLine(to: points[2])
        facet.closeSubpath()
        context.fill(
          facet,
          with: .color(.white.opacity(opacity * 0.055))
        )
      }

      context.stroke(
        path,
        with: .color(.white.opacity(opacity * 0.7)),
        lineWidth: pass == .near ? 1 : 0.65
      )
    }

    context.drawLayer { facets in
      facets.blendMode = .plusLighter
      for edge in PlayStation2MenuGeometry.crystalCrossEdges {
        var line = Path()
        line.move(to: projected[edge.0].point)
        line.addLine(to: projected[edge.1].point)
        facets.stroke(
          line,
          with: .color(accent.opacity(opacity * 0.18)),
          lineWidth: 0.55
        )
      }
    }
  }

  func drawCrystalCloudReflection(
    context: inout GraphicsContext,
    clippingPath: Path,
    bounds: CGRect,
    time: TimeInterval,
    index: Int,
    opacity: Double
  ) {
    guard bounds.width > 5, bounds.height > 5 else { return }

    let phase = time * (0.018 + Double(index % 4) * 0.004) + Double(index) * 1.37
    let color = theme.sharedColor(index: index + 1, time: time)
    let center = CGPoint(
      x: bounds.midX + CGFloat(sin(phase)) * bounds.width * 0.31,
      y: bounds.midY + CGFloat(cos(phase * 0.83)) * bounds.height * 0.24
    )
    let rect = CGRect(
      x: center.x - bounds.width * 0.52,
      y: center.y - bounds.height * 0.28,
      width: bounds.width * 1.04,
      height: bounds.height * 0.56
    )

    context.drawLayer { reflection in
      reflection.clip(to: clippingPath)
      reflection.blendMode = .plusLighter
      reflection.addFilter(.blur(radius: max(7, min(28, bounds.width * 0.12))))
      reflection.fill(
        Path(ellipseIn: rect),
        with: .color(color.opacity(opacity * 0.72))
      )
    }
  }

  func drawCrystalOrbReflections(
    context: inout GraphicsContext,
    clippingPath: Path,
    bounds: CGRect,
    time: TimeInterval,
    index: Int,
    reflectionCount: Int,
    opacity: Double
  ) {
    guard bounds.width > 8, bounds.height > 8 else { return }

    for reflectionIndex in 0..<reflectionCount {
      let orbIndex = (index + reflectionIndex * 2) % 4
      let color = theme.ribbonColor(index: orbIndex + 2, time: time)
      let moment = time * (0.055 + Double(orbIndex) * 0.006) + Double(index) * 0.71
      let yOffset = CGFloat(sin(moment + Double(reflectionIndex))) * bounds.height * 0.22
      let start = CGPoint(
        x: bounds.minX - bounds.width * 0.14,
        y: bounds.midY + yOffset
      )
      let control = CGPoint(
        x: bounds.midX + CGFloat(cos(moment * 0.77)) * bounds.width * 0.18,
        y: bounds.midY - CGFloat(cos(moment)) * bounds.height * 0.42
      )
      let end = CGPoint(
        x: bounds.maxX + bounds.width * 0.14,
        y: bounds.midY - yOffset * 0.7
      )
      var reflectionPath = Path()
      reflectionPath.move(to: start)
      reflectionPath.addQuadCurve(to: end, control: control)

      context.drawLayer { glow in
        glow.clip(to: clippingPath)
        glow.blendMode = .plusLighter
        glow.addFilter(.blur(radius: 3.5))
        glow.stroke(
          reflectionPath,
          with: .color(color.opacity(opacity * 0.95)),
          style: StrokeStyle(lineWidth: 2.2, lineCap: .round)
        )
      }

      context.drawLayer { reflection in
        reflection.clip(to: clippingPath)
        reflection.blendMode = .plusLighter
        reflection.stroke(
          reflectionPath,
          with: .color(color.opacity(opacity * 1.25)),
          style: StrokeStyle(lineWidth: 0.75, lineCap: .round)
        )

        let travelPhase =
          time * (0.1 + Double(orbIndex) * 0.008)
          + Double(index) * 0.43
          + Double(reflectionIndex) * 1.4
        let travel = 0.5 + sin(travelPhase) * 0.5
        let orbPoint = quadraticPoint(
          from: start,
          control: control,
          to: end,
          progress: CGFloat(travel)
        )
        let diameter = max(2.2, min(5.5, bounds.width * 0.045))
        reflection.fill(
          Path(
            ellipseIn: CGRect(
              x: orbPoint.x - diameter / 2,
              y: orbPoint.y - diameter / 2,
              width: diameter,
              height: diameter
            )
          ),
          with: .color(.white.opacity(opacity * 2.1))
        )
      }
    }
  }
}

// MARK: - PlayStation2MenuOrbRenderer

extension PlayStation2MenuBackground {
  func drawOrbTrails(
    context: inout GraphicsContext,
    camera: PlayStation2MenuCamera,
    time: TimeInterval,
    pass: PlayStation2MenuDepthPass
  ) {
    for index in 0..<4 {
      let configuration = orbConfiguration(index: index, camera: camera)
      let moment = time * configuration.speed + configuration.phase
      let headWorld = orbPoint(moment: moment, configuration: configuration)
      let head = camera.project(headWorld)
      let passVisibility = pass.visibility(at: head.depth)
      guard passVisibility > 0.001 else { continue }

      let samples = 42
      let projections = (0...samples).map { sample in
        let sampleMoment = moment - Double(samples - sample) * 0.027
        return camera.project(
          orbPoint(moment: sampleMoment, configuration: configuration)
        )
      }
      let points = projections.map(\.point)
      guard let tailPoint = points.first, let headPoint = points.last else {
        continue
      }
      let color = theme.ribbonColor(index: index + 2, time: time)
      var completePath = Path()
      completePath.move(to: tailPoint)
      let maximumTangentLength = min(camera.size.width, camera.size.height) * 0.08

      for sample in 0..<(points.count - 1) {
        let segment = OrbTrailGeometry.segment(
          points: points,
          index: sample,
          maximumTangentLength: maximumTangentLength
        )
        completePath.addCurve(
          to: segment.end,
          control1: segment.control1,
          control2: segment.control2
        )
      }

      let normalizedDepth = max(0, min(1, (head.depth + 0.7) / 1.4))
      let depthProgress = normalizedDepth * normalizedDepth * (3 - 2 * normalizedDepth)
      let depthOpacity = 0.18 + depthProgress * 0.68
      let renderedOpacity = depthOpacity * passVisibility
      let baseWidth = 0.7 + CGFloat(depthProgress) * 0.65

      context.drawLayer { glow in
        glow.blendMode = .plusLighter
        glow.addFilter(.blur(radius: CGFloat(4 + depthProgress * 2)))
        glow.stroke(
          completePath,
          with: .linearGradient(
            Gradient(stops: [
              .init(color: .clear, location: 0),
              .init(color: color.opacity(renderedOpacity * 0.04), location: 0.18),
              .init(color: color.opacity(renderedOpacity * 0.2), location: 0.62),
              .init(color: color.opacity(renderedOpacity * 0.46), location: 1),
            ]),
            startPoint: tailPoint,
            endPoint: headPoint
          ),
          style: StrokeStyle(
            lineWidth: baseWidth * 3,
            lineCap: .round,
            lineJoin: .round
          )
        )
      }

      context.drawLayer { trail in
        trail.blendMode = .plusLighter
        trail.stroke(
          completePath,
          with: .linearGradient(
            Gradient(stops: [
              .init(color: .clear, location: 0),
              .init(color: color.opacity(renderedOpacity * 0.06), location: 0.16),
              .init(color: color.opacity(renderedOpacity * 0.38), location: 0.58),
              .init(color: color.opacity(renderedOpacity), location: 1),
            ]),
            startPoint: tailPoint,
            endPoint: headPoint
          ),
          style: StrokeStyle(
            lineWidth: baseWidth,
            lineCap: .round,
            lineJoin: .round
          )
        )
      }

      let diameter = (5.2 + CGFloat(depthProgress) * 2.3) * head.scale
      let orbRect = CGRect(
        x: head.point.x - diameter / 2,
        y: head.point.y - diameter / 2,
        width: diameter,
        height: diameter
      )

      context.drawLayer { orb in
        orb.blendMode = .plusLighter
        orb.addFilter(
          .shadow(color: color.opacity(renderedOpacity), radius: 10)
        )
        orb.fill(
          Path(ellipseIn: orbRect),
          with: .color(
            .white.opacity((0.55 + depthOpacity * 0.45) * passVisibility)
          )
        )
      }
    }
  }

  func towerConfigurations(
    camera: PlayStation2MenuCamera
  ) -> [PlayStation2MenuTowerConfiguration] {
    [
      makeTower(
        x: -0.82, y: -0.58, depth: 0.64,
        width: 0.54, height: 0.54, shaft: 0.8,
        angle: 0.012, phase: 0.2, camera: camera
      ),
      makeTower(
        x: -0.84, y: -0.08, depth: 0.2,
        width: 0.38, height: 0.38, shaft: 0.5,
        angle: -0.018, phase: 1.1, camera: camera
      ),
      makeTower(
        x: -0.82, y: 0.34, depth: 0.46,
        width: 0.46, height: 0.46, shaft: 0.68,
        angle: 0.01, phase: 2.05, camera: camera
      ),
      makeTower(
        x: -0.42, y: -0.82, depth: 0.12,
        width: 0.44, height: 0.44, shaft: 0.62,
        angle: 0.016, phase: 4.0, camera: camera
      ),
      makeTower(
        x: -0.04, y: -0.84, depth: -0.35,
        width: 0.25, height: 0.25, shaft: 0.34,
        angle: -0.012, phase: 4.8, camera: camera
      ),
      makeTower(
        x: 0.72, y: -0.66, depth: 0.58,
        width: 0.52, height: 0.52, shaft: 0.74,
        angle: -0.016, phase: 5.7, camera: camera
      ),
      makeTower(
        x: 0.84, y: 0.02, depth: 0.24,
        width: 0.4, height: 0.4, shaft: 0.56,
        angle: 0.012, phase: 6.5, camera: camera
      ),
      makeTower(
        x: 0.24, y: 0.08, depth: -0.18,
        width: 0.2, height: 0.2, shaft: 0.18,
        angle: -0.01, phase: 7.1, camera: camera,
        isShortTower: true
      ),
      makeTower(
        x: 0.42, y: 0.34, depth: 0.06,
        width: 0.26, height: 0.26, shaft: 0.22,
        angle: 0.012, phase: 7.7, camera: camera,
        isShortTower: true
      ),
      makeTower(
        x: 0.58, y: 0.58, depth: 0.28,
        width: 0.23, height: 0.23, shaft: 0.16,
        angle: -0.014, phase: 8.3, camera: camera,
        isShortTower: true
      ),
      makeTower(
        x: 0.7, y: 0.82, depth: 0.4,
        width: 0.29, height: 0.29, shaft: 0.24,
        angle: 0.01, phase: 8.9, camera: camera,
        isShortTower: true
      ),
      makeTower(
        x: -0.46, y: 0.9, depth: 0.18,
        width: 0.36, height: 0.36, shaft: 0.48,
        angle: -0.012, phase: 8.2, camera: camera
      ),
      makeTower(
        x: -0.08, y: 0.91, depth: -0.12,
        width: 0.32, height: 0.32, shaft: 0.46,
        angle: 0.014, phase: 9.1, camera: camera
      ),
    ]
  }

  func makeTower(
    x: Double,
    y: Double,
    depth: Double,
    width: Double,
    height: Double,
    shaft: Double,
    angle: Double,
    phase: Double,
    camera: PlayStation2MenuCamera,
    isInner: Bool = false,
    isShortTower: Bool = false
  ) -> PlayStation2MenuTowerConfiguration {
    let variation = DynamicBackgroundMath.unit(abs(phase) * 0.37)

    return PlayStation2MenuTowerConfiguration(
      center: PlayStation2MenuVector3(
        x: x * camera.horizontalExtent,
        y: y * camera.verticalExtent,
        z: depth
      ),
      size: PlayStation2MenuVector3(
        x: width,
        y: height,
        z: shaft
      ),
      rotation: PlayStation2MenuRotation(
        x: 0,
        y: 0,
        z: angle
      ),
      pulseSpeed: 0.04 + variation * 0.025,
      pulseAmount: isShortTower
        ? 0.028
        : (isInner ? 0.025 : 0.035 + variation * 0.03),
      phase: phase,
      isInner: isInner
    )
  }

  func crystalConfigurations(
    camera: PlayStation2MenuCamera
  ) -> [PlayStation2MenuCrystalConfiguration] {
    let x = camera.horizontalExtent
    let y = camera.verticalExtent

    return [
      PlayStation2MenuCrystalConfiguration(
        center: PlayStation2MenuVector3(x: 0.08 * x, y: -0.3 * y, z: 0.16),
        side: 0.38,
        phase: 0.35,
        rotationSpeed: 0.035,
        tintIndex: 1
      ),
      PlayStation2MenuCrystalConfiguration(
        center: PlayStation2MenuVector3(x: -0.62 * x, y: -0.18 * y, z: -0.18),
        side: 0.23,
        phase: 1.7,
        rotationSpeed: 0.028,
        tintIndex: 3
      ),
      PlayStation2MenuCrystalConfiguration(
        center: PlayStation2MenuVector3(x: 0.68 * x, y: -0.58 * y, z: 0.28),
        side: 0.28,
        phase: 2.8,
        rotationSpeed: 0.032,
        tintIndex: 5
      ),
      PlayStation2MenuCrystalConfiguration(
        center: PlayStation2MenuVector3(x: 0.76 * x, y: 0.03 * y, z: -0.34),
        side: 0.2,
        phase: 4.2,
        rotationSpeed: 0.026,
        tintIndex: 7
      ),
      PlayStation2MenuCrystalConfiguration(
        center: PlayStation2MenuVector3(x: 0.82 * x, y: 0.67 * y, z: 0.72),
        side: 0.44,
        phase: 5.5,
        rotationSpeed: 0.024,
        tintIndex: 9
      ),
      PlayStation2MenuCrystalConfiguration(
        center: PlayStation2MenuVector3(x: -0.8 * x, y: 0.66 * y, z: 0.58),
        side: 0.36,
        phase: 3.9,
        rotationSpeed: 0.027,
        tintIndex: 11
      ),
      PlayStation2MenuCrystalConfiguration(
        center: PlayStation2MenuVector3(x: -0.73 * x, y: -0.68 * y, z: 0.42),
        side: 0.27,
        phase: 0.9,
        rotationSpeed: 0.03,
        tintIndex: 13
      ),
      PlayStation2MenuCrystalConfiguration(
        center: PlayStation2MenuVector3(x: 0.18 * x, y: 0.62 * y, z: -0.62),
        side: 0.18,
        phase: 2.15,
        rotationSpeed: 0.029,
        tintIndex: 15
      ),
    ]
  }

  func orbConfiguration(
    index: Int,
    camera: PlayStation2MenuCamera
  ) -> PlayStation2MenuOrbConfiguration {
    let seedA = DynamicBackgroundMath.seededUnit(index: index, salt: 23.13)
    let seedB = DynamicBackgroundMath.seededUnit(index: index, salt: 71.79)
    let seedC = DynamicBackgroundMath.seededUnit(index: index, salt: 109.11)
    let seedD = DynamicBackgroundMath.seededUnit(index: index, salt: 157.33)

    return PlayStation2MenuOrbConfiguration(
      center: PlayStation2MenuVector3(
        x: (-0.22 + seedA * 0.44) * camera.horizontalExtent,
        y: (-0.2 + seedB * 0.4) * camera.verticalExtent,
        z: 0
      ),
      radiusX: camera.horizontalExtent * (0.46 + seedC * 0.36),
      radiusY: camera.verticalExtent * (0.24 + seedD * 0.25),
      bend: camera.verticalExtent * (0.08 + seedA * 0.13),
      speed: 0.052 + seedB * 0.035,
      phase: seedC * .pi * 2,
      depthPhase: seedD * .pi * 2
    )
  }

  func orbPoint(
    moment: Double,
    configuration: PlayStation2MenuOrbConfiguration
  ) -> PlayStation2MenuVector3 {
    PlayStation2MenuVector3(
      x: configuration.center.x
        + cos(moment) * configuration.radiusX
        + sin(moment * 1.73 + configuration.depthPhase) * configuration.radiusX * 0.14,
      y: configuration.center.y
        + sin(moment * 0.78 + configuration.depthPhase * 0.2) * configuration.radiusY
        + cos(moment * 0.43) * configuration.bend,
      z: sin(moment * 0.83 + configuration.depthPhase) * 0.96
    )
  }

  func cuboidVertices(
    center: PlayStation2MenuVector3,
    halfSize: PlayStation2MenuVector3,
    rotation: PlayStation2MenuRotation,
    camera: PlayStation2MenuCamera
  ) -> [PlayStation2MenuProjection] {
    [
      PlayStation2MenuVector3(x: -halfSize.x, y: -halfSize.y, z: -halfSize.z),
      PlayStation2MenuVector3(x: halfSize.x, y: -halfSize.y, z: -halfSize.z),
      PlayStation2MenuVector3(x: halfSize.x, y: halfSize.y, z: -halfSize.z),
      PlayStation2MenuVector3(x: -halfSize.x, y: halfSize.y, z: -halfSize.z),
      PlayStation2MenuVector3(x: -halfSize.x, y: -halfSize.y, z: halfSize.z),
      PlayStation2MenuVector3(x: halfSize.x, y: -halfSize.y, z: halfSize.z),
      PlayStation2MenuVector3(x: halfSize.x, y: halfSize.y, z: halfSize.z),
      PlayStation2MenuVector3(x: -halfSize.x, y: halfSize.y, z: halfSize.z),
    ].map { local in
      let rotated =
        local
        .rotatedX(rotation.x)
        .rotatedY(rotation.y)
        .rotatedZ(rotation.z)
      return camera.project(center + rotated)
    }
  }

  func averageDepth(
    _ face: [Int],
    vertices: [PlayStation2MenuProjection]
  ) -> Double {
    face.reduce(0) { $0 + vertices[$1].depth } / Double(face.count)
  }

  func polygonPath(_ points: [CGPoint]) -> Path {
    var path = Path()
    guard let first = points.first else { return path }

    path.move(to: first)
    for point in points.dropFirst() {
      path.addLine(to: point)
    }
    path.closeSubpath()
    return path
  }

  func averagedPoint(_ points: [CGPoint]) -> CGPoint {
    guard !points.isEmpty else { return .zero }

    let total = points.reduce(CGPoint.zero) { result, point in
      CGPoint(x: result.x + point.x, y: result.y + point.y)
    }
    return CGPoint(
      x: total.x / CGFloat(points.count),
      y: total.y / CGFloat(points.count)
    )
  }

  func quadraticPoint(
    from start: CGPoint,
    control: CGPoint,
    to end: CGPoint,
    progress: CGFloat
  ) -> CGPoint {
    let inverse = 1 - progress
    return CGPoint(
      x: inverse * inverse * start.x
        + 2 * inverse * progress * control.x
        + progress * progress * end.x,
      y: inverse * inverse * start.y
        + 2 * inverse * progress * control.y
        + progress * progress * end.y
    )
  }
}

// MARK: - PlayStation3XMBMartGradient

struct PlayStation3XMBMartGradient {
  let startColor: Color
  let endColor: Color
  let angleDegrees: Double
  let offsetX: Double
  let offsetY: Double
  let width: Double
  let curvature: Double

  init(
    startColor: Color,
    endColor: Color,
    angleDegrees: Double,
    offsetX: Double = 0,
    offsetY: Double = 0,
    width: Double = 1,
    curvature: Double = 0
  ) {
    self.startColor = startColor
    self.endColor = endColor
    self.angleDegrees = angleDegrees
    self.offsetX = offsetX
    self.offsetY = offsetY
    self.width = width
    self.curvature = curvature
  }

  var stops: [Gradient.Stop] {
    (0...12).map { index in
      let progress = Double(index) / 12
      let smoothProgress = progress * progress * (3 - 2 * progress)
      return Gradient.Stop(
        color: blend(startColor, endColor, amount: smoothProgress),
        location: CGFloat(progress)
      )
    }
  }

  var startPoint: UnitPoint {
    let direction = gradientDirection
    let range = gradientRange(direction: direction)
    let centerProjection = 0.5 * direction.x + 0.5 * direction.y
    let offset = range.minimum - centerProjection
    return UnitPoint(
      x: CGFloat(0.5 + direction.x * offset),
      y: CGFloat(0.5 + direction.y * offset)
    )
  }

  var endPoint: UnitPoint {
    let direction = gradientDirection
    let range = gradientRange(direction: direction)
    let centerProjection = 0.5 * direction.x + 0.5 * direction.y
    let offset = range.maximum - centerProjection
    return UnitPoint(
      x: CGFloat(0.5 + direction.x * offset),
      y: CGFloat(0.5 + direction.y * offset)
    )
  }

  private var gradientDirection: (x: Double, y: Double) {
    let radians = angleDegrees * .pi / 180
    return (cos(radians), sin(radians))
  }

  private func gradientRange(
    direction: (x: Double, y: Double)
  ) -> (minimum: Double, maximum: Double) {
    let projections = [
      0,
      direction.x,
      direction.y,
      direction.x + direction.y,
    ]
    return (projections.min() ?? 0, projections.max() ?? 1)
  }

  private func blend(_ first: Color, _ second: Color, amount: Double) -> Color {
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

    return Color(
      red: Double(firstRed + (secondRed - firstRed) * amount),
      green: Double(firstGreen + (secondGreen - firstGreen) * amount),
      blue: Double(firstBlue + (secondBlue - firstBlue) * amount),
      opacity: Double(firstAlpha + (secondAlpha - firstAlpha) * amount)
    )
  }
}

struct PlayStation3XMBMartRGB {
  let red: Double
  let green: Double
  let blue: Double

  init(_ red: Int, _ green: Int, _ blue: Int) {
    self.red = Double(red) / 255
    self.green = Double(green) / 255
    self.blue = Double(blue) / 255
  }

  var color: Color {
    Color(red: red, green: green, blue: blue)
  }
}

extension PlayStation3XMBGradientPreset {
  var sourceGradient:
    (angleDegrees: Double, start: PlayStation3XMBMartRGB, end: PlayStation3XMBMartRGB)
  {
    switch self {
    case .theme, .original:
      return (90, PlayStation3XMBMartRGB(3, 8, 24), PlayStation3XMBMartRGB(37, 89, 179))
    case .januaryDay:
      return (90.25, PlayStation3XMBMartRGB(197, 197, 197), PlayStation3XMBMartRGB(201, 201, 201))
    case .januaryNight:
      return (89.75, PlayStation3XMBMartRGB(181, 181, 181), PlayStation3XMBMartRGB(0, 0, 0))
    case .februaryDay:
      return (67, PlayStation3XMBMartRGB(203, 158, 13), PlayStation3XMBMartRGB(219, 214, 41))
    case .februaryNight:
      return (93.75, PlayStation3XMBMartRGB(198, 188, 128), PlayStation3XMBMartRGB(0, 0, 0))
    case .marchDay:
      return (106, PlayStation3XMBMartRGB(142, 190, 40), PlayStation3XMBMartRGB(104, 168, 22))
    case .marchNight:
      return (90.25, PlayStation3XMBMartRGB(152, 170, 113), PlayStation3XMBMartRGB(0, 0, 0))
    case .aprilDay:
      return (136.75, PlayStation3XMBMartRGB(216, 182, 182), PlayStation3XMBMartRGB(231, 66, 117))
    case .aprilNight:
      return (90.25, PlayStation3XMBMartRGB(212, 174, 182), PlayStation3XMBMartRGB(10, 8, 8))
    case .mayDay:
      return (1.5, PlayStation3XMBMartRGB(19, 108, 19), PlayStation3XMBMartRGB(24, 156, 24))
    case .mayNight:
      return (116, PlayStation3XMBMartRGB(48, 118, 48), PlayStation3XMBMartRGB(11, 3, 11))
    case .juneDay:
      return (148.75, PlayStation3XMBMartRGB(198, 120, 238), PlayStation3XMBMartRGB(103, 77, 161))
    case .juneNight:
      return (91, PlayStation3XMBMartRGB(209, 163, 225), PlayStation3XMBMartRGB(0, 0, 0))
    case .julyDay:
      return (26.5, PlayStation3XMBMartRGB(0, 167, 146), PlayStation3XMBMartRGB(10, 240, 239))
    case .julyNight:
      return (109.75, PlayStation3XMBMartRGB(16, 129, 124), PlayStation3XMBMartRGB(17, 0, 0))
    case .augustDay:
      return (62.5, PlayStation3XMBMartRGB(0, 0, 95), PlayStation3XMBMartRGB(33, 217, 255))
    case .augustNight:
      return (69.5, PlayStation3XMBMartRGB(20, 159, 176), PlayStation3XMBMartRGB(0, 0, 31))
    case .septemberDay:
      return (148.5, PlayStation3XMBMartRGB(146, 44, 155), PlayStation3XMBMartRGB(217, 98, 236))
    case .septemberNight:
      return (51, PlayStation3XMBMartRGB(116, 0, 153), PlayStation3XMBMartRGB(12, 0, 11))
    case .octoberDay:
      return (128.5, PlayStation3XMBMartRGB(227, 151, 15), PlayStation3XMBMartRGB(224, 187, 2))
    case .octoberNight:
      return (89.75, PlayStation3XMBMartRGB(216, 142, 0), PlayStation3XMBMartRGB(0, 0, 0))
    case .novemberDay:
      return (90, PlayStation3XMBMartRGB(115, 68, 20), PlayStation3XMBMartRGB(154, 118, 47))
    case .novemberNight:
      return (90, PlayStation3XMBMartRGB(131, 86, 32), PlayStation3XMBMartRGB(18, 20, 17))
    case .decemberDay:
      return (170.5, PlayStation3XMBMartRGB(236, 68, 45), PlayStation3XMBMartRGB(214, 63, 43))
    case .decemberNight:
      return (118.25, PlayStation3XMBMartRGB(157, 59, 44), PlayStation3XMBMartRGB(0, 0, 3))
    }
  }
}

// MARK: - PlayStation3XMBMartSplinePipeline

// Direct Swift port of spline-reverse.js's synthetic descriptor and texture pipeline.
final class PlayStation3XMBMartSplinePipeline {
  private static let tableEntryCount = 0x169
  private static let loopIterations = 8
  private static let storesPerIteration = 8
  static let textureWidth = 256
  static let textureHeight = 64
  private static let descriptorByteCount = 0x2200
  private static let normA = [0.39584, -0.0052389996, -0.58664495, 0.189007]
  private static let normB = [-0.003751, -0.57536095, 0.161975, 0.417137]
  private static let registerWords = [0x00, 0x11, 0x22, 0x33]

  private var cachedDescriptorSeed: Int?
  private var descriptor = [UInt8](
    repeating: 0,
    count: PlayStation3XMBMartSplinePipeline.descriptorByteCount
  )
  private var runtimeInput = [Double](repeating: 0, count: 16)
  private var coefficients = [Double](
    repeating: 0,
    count: PlayStation3XMBMartSplinePipeline.tableEntryCount * 16
  )
  private var table = [Double](
    repeating: 0,
    count: PlayStation3XMBMartSplinePipeline.tableEntryCount * 4
  )
  private var kernelPacked = [Double](
    repeating: 0,
    count: PlayStation3XMBMartSplinePipeline.loopIterations
      * PlayStation3XMBMartSplinePipeline.storesPerIteration * 4
  )
  private var previousKernelPacked = [Double](
    repeating: 0,
    count: PlayStation3XMBMartSplinePipeline.loopIterations
      * PlayStation3XMBMartSplinePipeline.storesPerIteration * 4
  )
  private var controlPoints = [Double](repeating: 0, count: 28)
  private var displacement = [Double](
    repeating: 0,
    count: PlayStation3XMBMartSplinePipeline.textureWidth
      * PlayStation3XMBMartSplinePipeline.textureHeight
  )

  func update(settings: PlayStation3XMBSettings, time: TimeInterval) {
    buildRuntimeInputs(settings: settings)
    buildDescriptor(settings: settings)
    decodeCoefficients(settings: settings, time: time)
    buildSplineTable(settings: settings)
    runKernel(settings: settings, time: time)
    writeDisplacementTexture(settings: settings, time: time)
  }

  func sample(u: Double, v: Double) -> Double {
    let x = clamp(u, lower: 0, upper: 1) * Double(Self.textureWidth - 1)
    let y = clamp(v, lower: 0, upper: 1) * Double(Self.textureHeight - 1)
    let x0 = Int(floor(x))
    let y0 = Int(floor(y))
    let x1 = min(Self.textureWidth - 1, x0 + 1)
    let y1 = min(Self.textureHeight - 1, y0 + 1)
    let xBlend = x - floor(x)
    let yBlend = y - floor(y)
    let top = mix(
      displacement[y0 * Self.textureWidth + x0],
      displacement[y0 * Self.textureWidth + x1],
      amount: xBlend
    )
    let bottom = mix(
      displacement[y1 * Self.textureWidth + x0],
      displacement[y1 * Self.textureWidth + x1],
      amount: xBlend
    )
    return mix(top, bottom, amount: yBlend)
  }

  func copyDisplacement(into values: inout [Float]) {
    if values.count != displacement.count {
      values = Array(repeating: 0, count: displacement.count)
    }
    for index in displacement.indices {
      values[index] = Float(displacement[index])
    }
  }

  private func buildRuntimeInputs(settings: PlayStation3XMBSettings) {
    runtimeInput[0] = settings.damping
    runtimeInput[1] = settings.length
    runtimeInput[2] = settings.tension
    runtimeInput[3] = settings.spacing * 0.001
    runtimeInput[4] = settings.waveCosineAmplitude
    runtimeInput[5] = settings.waveBias
    runtimeInput[6] = settings.timeStep
    runtimeInput[7] = settings.perturbation
    runtimeInput[8] = settings.perturbationScale
    runtimeInput[9] = settings.flowSpeed
    runtimeInput[10] = settings.ffdScale1X
    runtimeInput[11] = settings.ffdScale2X
    runtimeInput[12] = settings.ffdOffsetY
    runtimeInput[13] = settings.fresnelScale
    runtimeInput[14] = settings.waveHeightScale
    runtimeInput[15] = 1
  }

  private func buildDescriptor(settings: PlayStation3XMBSettings) {
    let seed = Int(settings.reverseSyntheticDescriptorSeed)
    guard seed != cachedDescriptorSeed else { return }
    cachedDescriptorSeed = seed

    for index in descriptor.indices {
      let fraction = Double(index) / Double(descriptor.count)
      let wave =
        sin((fraction * 97) * (0.3 + settings.length)) * 0.5
        + sin((fraction * 211) * (0.15 + settings.tension * 3)) * 0.3
        + sin((fraction * 17) * (0.2 + settings.perturbationScale * 2)) * 0.2
      let noise = hash01(Double(index) * 13.37 + Double(seed) * 0.01) * 2 - 1
      let value = clamp(
        (wave * settings.reverseDescriptorStrength + noise * 0.35 + 1) * 0.5,
        lower: 0,
        upper: 1
      )
      descriptor[index] = UInt8(Int(value * 255))
    }
  }

  private func decodeCoefficients(
    settings: PlayStation3XMBSettings,
    time: TimeInterval
  ) {
    let descriptorStride = 0x130
    let phase =
      time * settings.flowSpeed * settings.timeStep
      * settings.reverseSyntheticDescriptorMotion

    for entry in 0..<Self.tableEntryCount {
      let entryBase = entry * 16
      let descriptorBase = (entry * descriptorStride) % descriptor.count

      for block in 0..<4 {
        for lane in 0..<4 {
          let byteIndex = (descriptorBase + block * 0x10 + lane * 4 + entry % 7) % descriptor.count
          let byteValue = Double(descriptor[byteIndex]) / 255
          let centered = byteValue * 2 - 1
          let harmonic = sin(
            Double(entry) * 0.07
              + Double(block) * 0.91
              + Double(lane) * 1.37
              + phase * 0.23
          )
          coefficients[entryBase + block * 4 + lane] =
            centered * 0.75 + harmonic * 0.25
        }
      }
    }
  }

  private func buildSplineTable(settings: PlayStation3XMBSettings) {
    for entry in 0..<Self.tableEntryCount {
      let coefficientBase = entry * 16
      let tableBase = entry * 4

      for lane in 0..<4 {
        let raw =
          coefficients[coefficientBase + lane] * runtimeInput[0]
          + coefficients[coefficientBase + 4 + lane] * runtimeInput[1]
          + coefficients[coefficientBase + 8 + lane] * runtimeInput[2]
          + coefficients[coefficientBase + 12 + lane] * runtimeInput[3]
        let denominatorMagnitude = max(abs(Self.normB[lane]), 0.05)
        let denominator =
          Self.normB[lane] < 0
          ? -denominatorMagnitude
          : denominatorMagnitude
        let normalized =
          ((raw - Self.normA[lane]) / denominator)
          * settings.reverseNormalizeGain
        table[tableBase + lane] = tanh(normalized)
      }
    }
  }

  private func runKernel(
    settings: PlayStation3XMBSettings,
    time: TimeInterval
  ) {
    for iteration in 0..<Self.loopIterations {
      let phase =
        time * settings.reverseKernelPhaseStep
        + Double(iteration) * 0.37
      var indices = [Double](repeating: 0, count: 4)

      for lane in 0..<4 {
        let word = (Self.registerWords[lane] + iteration * 0x13) & 0xff
        let baseIndex = tableIndex(from: word)
        let offset =
          sin(phase + Double(lane) * 0.77)
          * settings.reverseIndexJitter * Double(Self.tableEntryCount)
        indices[lane] = wrappedIndex(
          Double(baseIndex) + offset,
          length: Self.tableEntryCount
        )
      }

      let value0 = sampleTableVector(at: indices[0])
      let value1 = sampleTableVector(at: indices[1])
      let value2 = sampleTableVector(at: indices[2])
      let value3 = sampleTableVector(at: indices[3])
      let mixA = 0.5 + 0.5 * sin(phase * 0.7)
      let mixB = 0.5 + 0.5 * cos(phase * 0.9)
      let mixed12 = mixVectors(value0, value1, amount: mixA)
      let mixed13 = mixVectors(value1, value2, amount: mixB)
      let mixed14 = mixVectors(value2, value3, amount: mixA)
      let mixed15 = mixVectors(value3, value0, amount: mixB)
      let delta9 = subtractVectors(value1, value0)
      let delta4 = subtractVectors(value2, value1)
      let delta87 = subtractVectors(value3, value2)
      let delta89 = subtractVectors(value0, value3)
      let stores = [
        mixed12, mixed13, mixed14, mixed15,
        delta9, delta4, delta87, delta89,
      ]

      for storeIndex in 0..<stores.count {
        let packedBase = (iteration * Self.storesPerIteration + storeIndex) * 4
        for lane in 0..<4 {
          kernelPacked[packedBase + lane] = stores[storeIndex][lane]
        }
      }
    }

    let temporal = clamp(settings.reverseTemporalSmooth, lower: 0, upper: 0.999)
    for index in kernelPacked.indices {
      let smoothed =
        previousKernelPacked[index] * temporal
        + kernelPacked[index] * (1 - temporal)
      kernelPacked[index] = smoothed
      previousKernelPacked[index] = smoothed
    }
  }

  private func writeDisplacementTexture(
    settings: PlayStation3XMBSettings,
    time: TimeInterval
  ) {
    let controlPointCount = controlPoints.count
    let kernelVectorCount = Self.loopIterations * Self.storesPerIteration
    let flow = time * settings.flowSpeed * settings.timeStep

    for row in 0..<Self.textureHeight {
      let depth = Double(row) / Double(max(1, Self.textureHeight - 1)) * 2 - 1
      let rowBase = row * Self.textureWidth
      let rowPhase = flow * 0.25 + depth * 1.7

      for index in 0..<controlPointCount {
        let x = Double(index) / Double(max(1, controlPointCount - 1))
        let kernelPosition = Double(row) * 0.93 + Double(index) * 0.61 + flow * 0.35
        let lowerVector = positiveModulo(
          Int(floor(kernelPosition)),
          kernelVectorCount
        )
        let upperVector = (lowerVector + 1) % kernelVectorCount
        let vectorBlend = kernelPosition - floor(kernelPosition)
        let lowerBase = lowerVector * 4
        let upperBase = upperVector * 4
        let kernelX = mix(
          kernelPacked[lowerBase],
          kernelPacked[upperBase],
          amount: vectorBlend
        )
        let kernelY = mix(
          kernelPacked[lowerBase + 1],
          kernelPacked[upperBase + 1],
          amount: vectorBlend
        )
        let kernelZ = mix(
          kernelPacked[lowerBase + 2],
          kernelPacked[upperBase + 2],
          amount: vectorBlend
        )
        let kernelW = mix(
          kernelPacked[lowerBase + 3],
          kernelPacked[upperBase + 3],
          amount: vectorBlend
        )

        let reverseCore =
          (kernelX * 0.45
            + kernelY * 0.25
            + kernelZ * 0.2
            + kernelW * 0.1) * settings.reverseKernelGain
          + sin(rowPhase + x * 6.2) * settings.bandAmplitude
          + cos(
            depth * settings.bandSecondaryFrequency
              + x * 4.8
              + flow * 0.09
          ) * settings.bandSecondaryAmplitude
        let legacy =
          sin(x * .pi * 1.3 + depth * 0.8 - flow * settings.travelSpeed1)
          * settings.travelAmplitude1 * settings.tension
          + sin(x * .pi * 2.8 - depth * 1.2 + flow * settings.travelSpeed2)
          * settings.travelAmplitude2
          + settings.perturbation * settings.perturbationScale
          * sin(
            (x * (4 + settings.length * 2) + depth * 4 - flow * 0.6)
              * (settings.spacing * 0.01)
          )
        controlPoints[index] =
          reverseCore * settings.reversePipelineBlend
          + legacy * (1 - settings.reversePipelineBlend)
      }

      for xIndex in 0..<Self.textureWidth {
        displacement[rowBase + xIndex] = evaluateSpline(
          controlPoints: controlPoints,
          position: Double(xIndex) / Double(max(1, Self.textureWidth - 1))
        )
      }
    }
  }

  private func evaluateSpline(
    controlPoints: [Double],
    position: Double
  ) -> Double {
    let segmentCount = controlPoints.count - 3
    guard segmentCount >= 1 else { return 0 }
    let scaled = max(
      0,
      min(position * Double(segmentCount), Double(segmentCount) - 0.000_001)
    )
    let segment = Int(floor(scaled))
    let local = scaled - Double(segment)
    let squared = local * local
    let cubed = squared * local
    let basis = [
      (1 - 3 * local + 3 * squared - cubed) / 6,
      (4 - 6 * squared + 3 * cubed) / 6,
      (1 + 3 * local + 3 * squared - 3 * cubed) / 6,
      cubed / 6,
    ]
    return basis[0] * controlPoints[segment]
      + basis[1] * controlPoints[segment + 1]
      + basis[2] * controlPoints[segment + 2]
      + basis[3] * controlPoints[segment + 3]
  }

  private func tableIndex(from word: Int) -> Int {
    let high = (word >> 4) & 0xff
    let low = word & 0xf
    return (19 * high + low) % Self.tableEntryCount
  }

  private func wrappedIndex(_ value: Double, length: Int) -> Double {
    var result = value.truncatingRemainder(dividingBy: Double(length))
    if result < 0 {
      result += Double(length)
    }
    return result
  }

  private func sampleTableVector(at index: Double) -> [Double] {
    let lower = Int(floor(index)) % Self.tableEntryCount
    let upper = (lower + 1) % Self.tableEntryCount
    let blend = index - floor(index)
    let lowerBase = lower * 4
    let upperBase = upper * 4
    return (0..<4).map { lane in
      mix(
        table[lowerBase + lane],
        table[upperBase + lane],
        amount: blend
      )
    }
  }

  private func mixVectors(
    _ first: [Double],
    _ second: [Double],
    amount: Double
  ) -> [Double] {
    (0..<4).map { lane in
      mix(first[lane], second[lane], amount: amount)
    }
  }

  private func subtractVectors(
    _ first: [Double],
    _ second: [Double]
  ) -> [Double] {
    (0..<4).map { lane in first[lane] - second[lane] }
  }

  private func positiveModulo(_ value: Int, _ divisor: Int) -> Int {
    ((value % divisor) + divisor) % divisor
  }

  private func hash01(_ value: Double) -> Double {
    DynamicBackgroundMath.unit(sin(value) * 43_758.5453123)
  }
  private func mix(_ first: Double, _ second: Double, amount: Double) -> Double {
    first + (second - first) * amount
  }

  private func clamp(_ value: Double, lower: Double, upper: Double) -> Double {
    min(upper, max(lower, value))
  }
}

// MARK: - PlayStation3XMBByMartMetalRenderer

private struct XMBBackgroundUniforms {
  var startColor: SIMD4<Float>
  var endColor: SIMD4<Float>
  var directionRange: SIMD4<Float>
  var geometry: SIMD4<Float>
}

private struct XMBWaveUniforms {
  var timing: SIMD4<Float>
  var geometry: SIMD4<Float>
  var shaping: SIMD4<Float>
  var detail: SIMD4<Float>
  var ffdScale1: SIMD4<Float>
  var ffdScale2: SIMD4<Float>
  var ffdOffset: SIMD4<Float>
  var material: SIMD4<Float>
  var color: SIMD4<Float>
}

private struct XMBParticleUniforms {
  var timing: SIMD4<Float>
  var sizing: SIMD4<Float>
  var color: SIMD4<Float>
  var distribution: SIMD4<Float>
  var waveFollowing: SIMD4<Float>
}

private struct XMBSessionRandomNumberGenerator: RandomNumberGenerator {
  private var state: UInt64

  init(seed: UInt64) {
    state = seed == 0 ? 0x9e37_79b9_7f4a_7c15 : seed
  }

  mutating func next() -> UInt64 {
    state ^= state >> 12
    state ^= state << 25
    state ^= state >> 27
    return state &* 0x2545_f491_4f6c_dd1d
  }
}

private final class PlayStation3XMBMartMetalRenderer: NSObject, MTKViewDelegate {
  private let renderMode: PlayStation3XMBMartRenderMode
  private let sessionStartTime: TimeInterval
  private let particleTimeOffset: TimeInterval
  private let particleSeed: UInt64
  private let commandQueue: MTLCommandQueue
  private let backgroundPipeline: MTLRenderPipelineState
  private let wavePipeline: MTLRenderPipelineState
  private let particlePipeline: MTLRenderPipelineState
  private let waveVertexBuffer: MTLBuffer
  private let waveIndexBuffer: MTLBuffer
  private let waveIndexCount: Int
  private let splineTexture: MTLTexture
  private let splinePipeline = PlayStation3XMBMartSplinePipeline()

  private var settings = PlayStation3XMBSettings()
  private var theme: DynamicBackgroundTheme?
  private var particleControls = PlayStation3XMBMartParticleControls()
  private var backgroundUniforms = XMBBackgroundUniforms(
    startColor: SIMD4(0, 0, 0, 1),
    endColor: SIMD4(0.1, 0.3, 0.7, 1),
    directionRange: SIMD4(0, 1, 0, 1),
    geometry: SIMD4(0, 0, 1, 0)
  )
  private var waveColor = SIMD4<Float>(1, 1, 1, 1)
  private var particleBuffer: MTLBuffer?
  private var particleCount = 0
  private var displacementValues = [Float](
    repeating: 0,
    count: PlayStation3XMBMartSplinePipeline.textureWidth
      * PlayStation3XMBMartSplinePipeline.textureHeight
  )
  private var splineTime: TimeInterval = 0
  private var particleTime: TimeInterval = 0

  @MainActor
  init?(
    view: MTKView,
    renderMode: PlayStation3XMBMartRenderMode,
    sessionStartTime: TimeInterval
  ) {
    guard let device = view.device,
      let commandQueue = device.makeCommandQueue(),
      let library = PlayStation3XMBByMartShaderLibrary.makeLibrary(device: device),
      let backgroundVertex = library.makeFunction(name: "xmbBackgroundVertex"),
      let backgroundFragment = library.makeFunction(name: "xmbBackgroundFragment"),
      let waveVertex = library.makeFunction(name: "xmbWaveVertex"),
      let waveFragment = library.makeFunction(name: "xmbWaveFragment"),
      let particleVertex = library.makeFunction(name: "xmbParticleVertex"),
      let particleFragment = library.makeFunction(name: "xmbParticleFragment")
    else {
      return nil
    }

    let backgroundDescriptor = MTLRenderPipelineDescriptor()
    backgroundDescriptor.vertexFunction = backgroundVertex
    backgroundDescriptor.fragmentFunction = backgroundFragment
    backgroundDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat

    let waveDescriptor = MTLRenderPipelineDescriptor()
    waveDescriptor.vertexFunction = waveVertex
    waveDescriptor.fragmentFunction = waveFragment
    waveDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat
    waveDescriptor.colorAttachments[0].isBlendingEnabled = true
    waveDescriptor.colorAttachments[0].sourceRGBBlendFactor = .sourceAlpha
    waveDescriptor.colorAttachments[0].destinationRGBBlendFactor = .oneMinusSourceAlpha
    waveDescriptor.colorAttachments[0].sourceAlphaBlendFactor = .one
    waveDescriptor.colorAttachments[0].destinationAlphaBlendFactor = .oneMinusSourceAlpha

    let particleDescriptor = MTLRenderPipelineDescriptor()
    particleDescriptor.vertexFunction = particleVertex
    particleDescriptor.fragmentFunction = particleFragment
    particleDescriptor.colorAttachments[0].pixelFormat = view.colorPixelFormat
    particleDescriptor.colorAttachments[0].isBlendingEnabled = true
    particleDescriptor.colorAttachments[0].sourceRGBBlendFactor = .one
    particleDescriptor.colorAttachments[0].destinationRGBBlendFactor = .one
    particleDescriptor.colorAttachments[0].sourceAlphaBlendFactor = .one
    particleDescriptor.colorAttachments[0].destinationAlphaBlendFactor = .one

    guard
      let backgroundPipeline = try? device.makeRenderPipelineState(
        descriptor: backgroundDescriptor
      ),
      let wavePipeline = try? device.makeRenderPipelineState(
        descriptor: waveDescriptor
      ),
      let particlePipeline = try? device.makeRenderPipelineState(
        descriptor: particleDescriptor
      )
    else {
      return nil
    }

    let grid = Self.makeWaveGrid(resolution: 100)
    guard
      let waveVertexBuffer = device.makeBuffer(
        bytes: grid.vertices,
        length: MemoryLayout<SIMD2<Float>>.stride * grid.vertices.count,
        options: .storageModeShared
      ),
      let waveIndexBuffer = device.makeBuffer(
        bytes: grid.indices,
        length: MemoryLayout<UInt16>.stride * grid.indices.count,
        options: .storageModeShared
      )
    else {
      return nil
    }

    let textureDescriptor = MTLTextureDescriptor.texture2DDescriptor(
      pixelFormat: .r32Float,
      width: PlayStation3XMBMartSplinePipeline.textureWidth,
      height: PlayStation3XMBMartSplinePipeline.textureHeight,
      mipmapped: false
    )
    textureDescriptor.usage = .shaderRead
    textureDescriptor.storageMode = .shared
    guard let splineTexture = device.makeTexture(descriptor: textureDescriptor) else {
      return nil
    }

    self.renderMode = renderMode
    self.sessionStartTime = sessionStartTime
    self.particleTimeOffset =
      sessionStartTime.truncatingRemainder(dividingBy: 1000)
    let renderModeSalt: UInt64 =
      switch renderMode {
      case .fullBackground: 0x1465_0fb0_739d_0383
      case .foregroundOnly: 0x9e37_79b9_7f4a_7c15
      case .particlesOnly: 0xd1b5_4a32_d192_ed03
      }
    self.particleSeed = sessionStartTime.bitPattern ^ renderModeSalt
    self.commandQueue = commandQueue
    self.backgroundPipeline = backgroundPipeline
    self.wavePipeline = wavePipeline
    self.particlePipeline = particlePipeline
    self.waveVertexBuffer = waveVertexBuffer
    self.waveIndexBuffer = waveIndexBuffer
    self.waveIndexCount = grid.indices.count
    self.splineTexture = splineTexture
    super.init()

    rebuildParticles(device: device, count: Int(settings.particleCount))
  }

  func update(
    settings: PlayStation3XMBSettings,
    theme: DynamicBackgroundTheme,
    particleControls: PlayStation3XMBMartParticleControls
  ) {
    self.settings = settings
    self.theme = theme
    self.particleControls = particleControls

    let density =
      renderMode == .particlesOnly
      ? particleControls.amount
      : settings.particleVerticalDensity
    let desiredCount = Int(
      min(
        4000,
        max(10, (settings.particleCount * density).rounded())
      )
    )
    if desiredCount != particleCount {
      rebuildParticles(device: commandQueue.device, count: desiredCount)
    }
  }

  func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

  func draw(in view: MTKView) {
    let elapsed = max(0, CFAbsoluteTimeGetCurrent() - sessionStartTime)
    splineTime = elapsed
    particleTime = particleTimeOffset + elapsed

    if let theme {
      let paletteTime = CFAbsoluteTimeGetCurrent()
      if renderMode == .fullBackground {
        let gradient = PlayStation3XMBMartThemeResolver.backgroundGradient(
          theme: theme,
          settings: settings,
          time: paletteTime
        )
        backgroundUniforms = makeBackgroundUniforms(gradient: gradient)
      }
      waveColor = colorComponents(
        theme.highlightColor(index: 0, time: paletteTime)
      )
    }

    if renderMode != .particlesOnly {
      splinePipeline.update(settings: settings, time: splineTime)
      splinePipeline.copyDisplacement(into: &displacementValues)
      displacementValues.withUnsafeBytes { bytes in
        guard let baseAddress = bytes.baseAddress else { return }
        splineTexture.replace(
          region: MTLRegionMake2D(
            0,
            0,
            PlayStation3XMBMartSplinePipeline.textureWidth,
            PlayStation3XMBMartSplinePipeline.textureHeight
          ),
          mipmapLevel: 0,
          withBytes: baseAddress,
          bytesPerRow: MemoryLayout<Float>.stride
            * PlayStation3XMBMartSplinePipeline.textureWidth
        )
      }
    }

    guard let descriptor = view.currentRenderPassDescriptor,
      let drawable = view.currentDrawable,
      let commandBuffer = commandQueue.makeCommandBuffer(),
      let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: descriptor)
    else {
      return
    }

    if renderMode == .fullBackground {
      encodeBackground(encoder: encoder)
    }
    if renderMode != .particlesOnly {
      encodeWave(encoder: encoder)
    }
    encodeParticles(encoder: encoder, drawableSize: view.drawableSize)
    encoder.endEncoding()
    commandBuffer.present(drawable)
    commandBuffer.commit()
  }

  private func encodeBackground(encoder: MTLRenderCommandEncoder) {
    var uniforms = backgroundUniforms
    encoder.setRenderPipelineState(backgroundPipeline)
    encoder.setFragmentBytes(
      &uniforms,
      length: MemoryLayout<XMBBackgroundUniforms>.stride,
      index: 0
    )
    encoder.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4)
  }

  private func encodeWave(encoder: MTLRenderCommandEncoder) {
    var uniforms = makeWaveUniforms()

    encoder.setRenderPipelineState(wavePipeline)
    encoder.setCullMode(.none)
    encoder.setVertexBuffer(waveVertexBuffer, offset: 0, index: 0)
    encoder.setVertexBytes(
      &uniforms,
      length: MemoryLayout<XMBWaveUniforms>.stride,
      index: 1
    )
    encoder.setFragmentBytes(
      &uniforms,
      length: MemoryLayout<XMBWaveUniforms>.stride,
      index: 1
    )
    encoder.setVertexTexture(splineTexture, index: 0)
    encoder.drawIndexedPrimitives(
      type: .triangleStrip,
      indexCount: waveIndexCount,
      indexType: .uint16,
      indexBuffer: waveIndexBuffer,
      indexBufferOffset: 0
    )
  }

  private func encodeParticles(
    encoder: MTLRenderCommandEncoder,
    drawableSize: CGSize
  ) {
    guard particleCount > 0, let particleBuffer else { return }
    let rendersParticleOverlay = renderMode == .particlesOnly
    let aspect = Float(drawableSize.width / max(1, drawableSize.height))
    let ratio = max(1, min(aspect, 2)) * 0.375
    var particleColor = waveColor
    particleColor.w =
      rendersParticleOverlay
      ? Float(particleControls.depthVariation)
      : Float(settings.particleDepthVariation)
    var uniforms = XMBParticleUniforms(
      timing: SIMD4(
        Float(particleTime),
        Float(settings.particleFlowSpeed * particleControls.speed),
        ratio,
        Float(
          settings.particleOpacity
            * particleControls.brightness
            * particleControls.opacity
        )
      ),
      sizing: SIMD4(
        Float(settings.particleSizeBase),
        Float(settings.particleSizeVariance),
        rendersParticleOverlay ? Float(1.5 * particleControls.size) : 1,
        rendersParticleOverlay
          ? Float(1 - particleControls.verticalLevel * 2)
          : 0
      ),
      color: particleColor,
      distribution: SIMD4(
        rendersParticleOverlay
          ? Float(particleControls.direction * 2)
          : 0,
        rendersParticleOverlay
          ? Float(particleControls.dispersion * particleControls.verticalSpread)
          : Float(settings.particleVerticalSpread),
        rendersParticleOverlay ? 1 : 0,
        rendersParticleOverlay
          ? Float(particleControls.outerDispersion)
          : Float(settings.particleOuterDispersion)
      ),
      waveFollowing: SIMD4(
        !rendersParticleOverlay && settings.particlesFollowWaveY ? 1 : 0,
        renderMode == .foregroundOnly ? 1 : 0,
        0,
        0
      )
    )
    var waveUniforms = makeWaveUniforms()

    encoder.setRenderPipelineState(particlePipeline)
    encoder.setVertexBuffer(particleBuffer, offset: 0, index: 0)
    encoder.setVertexBytes(
      &uniforms,
      length: MemoryLayout<XMBParticleUniforms>.stride,
      index: 1
    )
    encoder.setVertexBytes(
      &waveUniforms,
      length: MemoryLayout<XMBWaveUniforms>.stride,
      index: 2
    )
    encoder.setVertexTexture(splineTexture, index: 0)
    encoder.setFragmentBytes(
      &uniforms,
      length: MemoryLayout<XMBParticleUniforms>.stride,
      index: 1
    )
    encoder.drawPrimitives(
      type: .point,
      vertexStart: 0,
      vertexCount: particleCount
    )
  }

  private func makeWaveUniforms() -> XMBWaveUniforms {
    XMBWaveUniforms(
      timing: SIMD4(
        Float(splineTime),
        Float(settings.flowSpeed),
        Float(settings.tension),
        Float(settings.damping)
      ),
      geometry: SIMD4(
        Float(settings.length),
        Float(settings.spacing),
        Float(settings.perturbation),
        Float(settings.perturbationScale)
      ),
      shaping: SIMD4(
        Float(settings.timeStep),
        Float(settings.waveCosineAmplitude),
        Float(settings.waveBias),
        Float(settings.waveHeightScale)
      ),
      detail: SIMD4(
        Float(settings.waveSoftClip),
        Float(settings.ffdYAmplitude),
        Float(settings.ffdZAmplitude),
        Float(settings.zDetailScale)
      ),
      ffdScale1: SIMD4(
        Float(settings.ffdScale1X),
        Float(settings.ffdScale1Y),
        Float(settings.ffdScale1Z),
        0
      ),
      ffdScale2: SIMD4(
        Float(settings.ffdScale2X),
        Float(settings.ffdScale2Y),
        Float(settings.ffdScale2Z),
        0
      ),
      ffdOffset: SIMD4(
        Float(settings.ffdOffsetX),
        Float(settings.ffdOffsetY),
        Float(settings.ffdOffsetZ),
        0
      ),
      material: SIMD4(
        Float(settings.opacity),
        Float(settings.brightness),
        Float(settings.fresnelPower),
        Float(settings.fresnelScale)
      ),
      color: waveColor
    )
  }

  private func rebuildParticles(device: MTLDevice, count: Int) {
    var generator = XMBSessionRandomNumberGenerator(
      seed: particleSeed ^ UInt64(count)
    )
    let seeds = (0..<count).map { _ in
      SIMD3<Float>(
        Float.random(in: 0..<1, using: &generator),
        Float.random(in: 0..<1, using: &generator),
        pow(Float.random(in: 0..<1, using: &generator), 8) + 0.1
      )
    }
    particleBuffer = device.makeBuffer(
      bytes: seeds,
      length: MemoryLayout<SIMD3<Float>>.stride * seeds.count,
      options: .storageModeShared
    )
    particleCount = count
  }

  private func makeBackgroundUniforms(
    gradient: PlayStation3XMBMartGradient
  ) -> XMBBackgroundUniforms {
    let radians = Float(gradient.angleDegrees * .pi / 180)
    let direction = SIMD2<Float>(cos(radians), sin(radians))
    let projections: [Float] = [
      0,
      direction.x,
      direction.y,
      direction.x + direction.y,
    ]
    let minimum = projections.min() ?? 0
    let maximum = projections.max() ?? 1
    return XMBBackgroundUniforms(
      startColor: colorComponents(gradient.startColor),
      endColor: colorComponents(gradient.endColor),
      directionRange: SIMD4(
        direction.x,
        direction.y,
        minimum,
        max(0.000_001, maximum - minimum)
      ),
      geometry: SIMD4(
        Float(gradient.offsetX),
        Float(gradient.offsetY),
        Float(max(0.05, gradient.width)),
        Float(min(1, max(-1, gradient.curvature)))
      )
    )
  }

  private func colorComponents(_ color: Color) -> SIMD4<Float> {
    let uiColor = UIColor(color)
    var red: CGFloat = 0
    var green: CGFloat = 0
    var blue: CGFloat = 0
    var alpha: CGFloat = 0
    guard uiColor.getRed(&red, green: &green, blue: &blue, alpha: &alpha) else {
      return SIMD4(1, 1, 1, 1)
    }
    return SIMD4(Float(red), Float(green), Float(blue), Float(alpha))
  }

  private static func makeWaveGrid(
    resolution: Int
  ) -> (vertices: [SIMD2<Float>], indices: [UInt16]) {
    let strips = resolution - 1
    let verticesPerStrip = resolution * 2
    var vertices: [SIMD2<Float>] = []
    vertices.reserveCapacity(strips * verticesPerStrip)

    for y in 0..<strips {
      for x in 0..<resolution {
        let horizontal = Float(x) / Float(resolution - 1) * 2 - 1
        vertices.append(
          SIMD2(
            horizontal,
            Float(y + 1) / Float(resolution - 1) * 2 - 1
          )
        )
        vertices.append(
          SIMD2(
            horizontal,
            Float(y) / Float(resolution - 1) * 2 - 1
          )
        )
      }
    }

    var indices: [UInt16] = []
    indices.reserveCapacity(strips * (verticesPerStrip + 2) - 2)
    var base = 0
    for strip in 0..<strips {
      if strip > 0 {
        indices.append(UInt16(base - 1))
        indices.append(UInt16(base))
      }
      for index in 0..<verticesPerStrip {
        indices.append(UInt16(base + index))
      }
      base += verticesPerStrip
    }
    return (vertices, indices)
  }
}

// MARK: - PlayStation3XMBMartMetalSurface

enum PlayStation3XMBMartRenderMode: Equatable {
  case fullBackground
  case foregroundOnly
  case particlesOnly
}

struct PlayStation3XMBMartParticleControls {
  var amount = 1.0
  var speed = 1.0
  var direction = 0.0
  var dispersion = 1.0
  var verticalSpread = 1.0
  var outerDispersion = 0.0
  var verticalLevel = 0.5
  var size = 1.0
  var brightness = 1.0
  var opacity = 1.0
  var depthVariation = 0.0
}

struct PlayStation3XMBMartMetalSurface: UIViewRepresentable {
  let settings: PlayStation3XMBSettings
  let theme: DynamicBackgroundTheme
  let sessionStartTime: TimeInterval
  var renderMode: PlayStation3XMBMartRenderMode = .fullBackground
  var particleControls = PlayStation3XMBMartParticleControls()

  func makeCoordinator() -> Coordinator {
    Coordinator(renderMode: renderMode, sessionStartTime: sessionStartTime)
  }

  func makeUIView(context: Context) -> MTKView {
    let view = MTKView(frame: .zero, device: MTLCreateSystemDefaultDevice())
    view.colorPixelFormat = .bgra8Unorm
    let rendersTransparentSurface = renderMode != .fullBackground
    view.clearColor = MTLClearColor(
      red: 0,
      green: 0,
      blue: 0,
      alpha: rendersTransparentSurface ? 0 : 1
    )
    view.framebufferOnly = true
    view.autoResizeDrawable = true
    view.preferredFramesPerSecond = 30
    view.enableSetNeedsDisplay = false
    view.isPaused = false
    view.isOpaque = !rendersTransparentSurface
    view.layer.isOpaque = !rendersTransparentSurface
    view.backgroundColor = .clear
    context.coordinator.attach(to: view)
    context.coordinator.renderer?.update(
      settings: settings,
      theme: theme,
      particleControls: particleControls
    )
    return view
  }

  func updateUIView(_ view: MTKView, context: Context) {
    context.coordinator.renderer?.update(
      settings: settings,
      theme: theme,
      particleControls: particleControls
    )
  }

  static func dismantleUIView(_ view: MTKView, coordinator: Coordinator) {
    view.delegate = nil
    view.isPaused = true
    view.enableSetNeedsDisplay = true
    view.releaseDrawables()
    coordinator.detach()
    view.device = nil
  }

  final class Coordinator {
    fileprivate var renderer: PlayStation3XMBMartMetalRenderer?
    private let renderMode: PlayStation3XMBMartRenderMode
    private let sessionStartTime: TimeInterval
    private var retainsShaderLibrary = false

    init(
      renderMode: PlayStation3XMBMartRenderMode,
      sessionStartTime: TimeInterval
    ) {
      self.renderMode = renderMode
      self.sessionStartTime = sessionStartTime
    }

    @MainActor
    func attach(to view: MTKView) {
      guard renderer == nil else { return }
      guard let renderer = PlayStation3XMBMartMetalRenderer(
        view: view,
        renderMode: renderMode,
        sessionStartTime: sessionStartTime
      ) else {
        PlayStation3XMBByMartShaderLibrary.releaseIfUnused()
        return
      }
      self.renderer = renderer
      PlayStation3XMBByMartShaderLibrary.retainForRenderer()
      retainsShaderLibrary = true
      view.delegate = renderer
    }

    @MainActor
    func detach() {
      renderer = nil
      guard retainsShaderLibrary else {
        PlayStation3XMBByMartShaderLibrary.releaseIfUnused()
        return
      }
      retainsShaderLibrary = false
      PlayStation3XMBByMartShaderLibrary.releaseForRenderer()
    }
  }
}

// MARK: - PlayStation3XMBByMartBackground

// Native SwiftUI adaptation of Mart's MIT-licensed PlayStation 3 XMB recreation.
struct PlayStation3XMBByMartBackground: View {
  let theme: DynamicBackgroundTheme
  @Environment(\.menuBackgroundSessionStart) private var menuBackgroundSessionStart

  @ViewBuilder
  var body: some View {
    if usesPlayStation4PaletteBackdrop {
      ZStack {
        PlayStation4WavesBackground(theme: theme, showsWaves: false)

        PlayStation3XMBMartMetalSurface(
          settings: settings,
          theme: theme,
          sessionStartTime: menuBackgroundSessionStart.timeIntervalSinceReferenceDate,
          renderMode: .foregroundOnly
        )
      }
      .ignoresSafeArea()
      .accessibilityHidden(true)
    } else {
      PlayStation3XMBMartMetalSurface(
        settings: settings,
        theme: theme,
        sessionStartTime: menuBackgroundSessionStart.timeIntervalSinceReferenceDate
      )
      .ignoresSafeArea()
      .accessibilityHidden(true)
    }
  }

  private var settings: PlayStation3XMBSettings {
    theme.particleSettings.playStation3XMB
  }

  private var usesPlayStation4PaletteBackdrop: Bool {
    settings.gradientPreset == .theme && theme.usesDynamicSharedPalette
  }
}

private enum PlayStation3XMBMartThemeResolver {
  static func backgroundGradient(
    theme: DynamicBackgroundTheme,
    settings: PlayStation3XMBSettings,
    time: TimeInterval
  ) -> PlayStation3XMBMartGradient {
    if settings.gradientPreset == .theme {
      return PlayStation3XMBMartGradient(
        startColor: themedGradientColor(
          theme.sharedColor(index: 0, time: time),
          theme: theme,
          multiplier: settings.gradientTopMul,
          blueMultiplier: 1.2
        ),
        endColor: themedGradientColor(
          theme.sharedColor(index: 2, time: time),
          theme: theme,
          multiplier: settings.gradientBotMul
        ),
        angleDegrees: 90 + theme.sharedPaletteGradientTilt,
        offsetX: theme.particleSettings.sharedPaletteGradientOffsetX,
        offsetY: theme.particleSettings.sharedPaletteGradientOffsetY,
        width: theme.particleSettings.sharedPaletteGradientWidth,
        curvature: theme.sharedPaletteGradientCurvature
      )
    }

    if settings.gradientPreset == .original {
      let original = Color(
        red: settings.colorR / 255,
        green: settings.colorG / 255,
        blue: settings.colorB / 255
      )
      return PlayStation3XMBMartGradient(
        startColor: scaledColor(
          original,
          multiplier: settings.gradientTopMul,
          blueMultiplier: 1.2
        ),
        endColor: scaledColor(original, multiplier: settings.gradientBotMul),
        angleDegrees: 90 + theme.sharedPaletteGradientTilt,
        offsetX: theme.particleSettings.sharedPaletteGradientOffsetX,
        offsetY: theme.particleSettings.sharedPaletteGradientOffsetY,
        width: theme.particleSettings.sharedPaletteGradientWidth,
        curvature: theme.sharedPaletteGradientCurvature
      )
    }

    let preset = settings.gradientPreset.sourceGradient
    return PlayStation3XMBMartGradient(
      startColor: preset.start.color,
      endColor: preset.end.color,
      angleDegrees: preset.angleDegrees + theme.sharedPaletteGradientTilt,
      offsetX: theme.particleSettings.sharedPaletteGradientOffsetX,
      offsetY: theme.particleSettings.sharedPaletteGradientOffsetY,
      width: theme.particleSettings.sharedPaletteGradientWidth,
      curvature: theme.sharedPaletteGradientCurvature
    )
  }

  private static func themedGradientColor(
    _ color: Color,
    theme: DynamicBackgroundTheme,
    multiplier: Double,
    blueMultiplier: Double = 1
  ) -> Color {
    let brightenedColor = scaledColor(
      color,
      multiplier: max(1, multiplier),
      blueMultiplier: blueMultiplier
    )
    return theme.paletteBackgroundColor(
      brightenedColor,
      darkness: max(0, 1 - multiplier)
    )
  }

  private static func scaledColor(
    _ color: Color,
    multiplier: Double,
    blueMultiplier: Double = 1
  ) -> Color {
    let uiColor = UIColor(color)
    var red: CGFloat = 0
    var green: CGFloat = 0
    var blue: CGFloat = 0
    var alpha: CGFloat = 0

    guard uiColor.getRed(&red, green: &green, blue: &blue, alpha: &alpha) else {
      return color.opacity(DynamicBackgroundMath.clamp(multiplier, to: 0...1))
    }

    return Color(
      red: DynamicBackgroundMath.clamp(Double(red) * multiplier, to: 0...1),
      green: DynamicBackgroundMath.clamp(Double(green) * multiplier, to: 0...1),
      blue: DynamicBackgroundMath.clamp(
        Double(blue) * multiplier * blueMultiplier,
        to: 0...1
      ),
      opacity: Double(alpha)
    )
  }

}

// MARK: - FaceButtonsBackground

struct FaceButtonsBackground: View {
  let theme: DynamicBackgroundTheme
  var showsBackdrop = true
  var showsLightBands = true

  var body: some View {
    TimelineView(.animation(minimumInterval: 1 / 30)) { timeline in
      let time = timeline.date.timeIntervalSinceReferenceDate
      let motionTime =
        time
        * settings.faceButtonSpeed
        * settings.backgrounds.faceButtonsSpeed
      let deepColor = theme.sharedColor(index: 0, time: time)
      let midColor = theme.sharedColor(index: 1, time: time)
      let lightColor = theme.sharedColor(index: 2, time: time)
      let sharedGradient = theme.sharedGradientPoints(
        from: .topLeading,
        to: .bottomTrailing
      )

      ZStack {
        if showsBackdrop {
          PaletteGradientField(
            colors: [
              theme.paletteBackgroundColor(deepColor, darkness: 0.96),
              theme.paletteBackgroundColor(deepColor, darkness: 0.54),
              theme.paletteBackgroundColor(midColor, darkness: 0.66),
              theme.paletteBackgroundColor(midColor, darkness: 0.94),
            ],
            startPoint: sharedGradient.start,
            endPoint: sharedGradient.end,
            curvature: theme.sharedPaletteGradientCurvature
          )

          Circle()
            .fill(lightColor.opacity(0.18))
            .frame(width: 780, height: 780)
            .blur(radius: 160)
            .offset(
              x: CGFloat(sin(motionTime * 0.24)) * 90,
              y: CGFloat(cos(motionTime * 0.18)) * 70
            )
        }

        Canvas { context, size in
          if showsLightBands && settings.backgrounds.faceButtonsShowsLightBands {
            drawWeavingLightBands(
              context: &context,
              size: size,
              motionTime: motionTime,
              colorTime: time
            )
          }
          drawFaceButtonField(
            context: &context,
            size: size,
            motionTime: motionTime,
            colorTime: time
          )
        }

        if showsBackdrop {
          LinearGradient(
            colors: [
              theme.paletteDarkOverlay(opacity: 0.18),
              .clear,
              theme.paletteDarkOverlay(opacity: 0.38),
            ],
            startPoint: .top,
            endPoint: .bottom
          )
        }
      }
      .ignoresSafeArea()
    }
  }

  private var settings: DynamicParticleSettings {
    theme.particleSettings
  }

  // Draws soft sine-wave paths that the face symbols appear to weave through.
  private func drawWeavingLightBands(
    context: inout GraphicsContext,
    size: CGSize,
    motionTime: TimeInterval,
    colorTime: TimeInterval
  ) {
    for bandIndex in 0..<5 {
      var path = Path()
      let steps = 52
      let baseY = size.height * CGFloat(0.2 + Double(bandIndex) * 0.14)
      let amplitude = size.height * CGFloat(0.035 + Double(bandIndex % 2) * 0.018)
      let phase =
        motionTime * (0.18 + Double(bandIndex) * 0.025)
        + Double(bandIndex) * 1.24

      for step in 0...steps {
        let progress = CGFloat(step) / CGFloat(steps)
        let x = size.width * progress
        let y =
          baseY
          + sin(Double(progress) * .pi * 3.4 + phase) * amplitude
          + cos(Double(progress) * .pi * 1.6 - phase * 0.7) * amplitude * 0.45
        let point = CGPoint(x: x, y: y)

        if step == 0 {
          path.move(to: point)
        } else {
          path.addLine(to: point)
        }
      }

      let color = theme.ribbonColor(index: bandIndex, time: colorTime)
      context.drawLayer { glow in
        glow.addFilter(.blur(radius: 16))
        glow.stroke(
          path,
          with: .color(color.opacity(0.12)),
          style: StrokeStyle(lineWidth: 14, lineCap: .round, lineJoin: .round)
        )
      }
      context.stroke(
        path,
        with: .color(color.opacity(0.3)),
        style: StrokeStyle(lineWidth: 0.9, lineCap: .round, lineJoin: .round)
      )
    }
  }

  // Draws face-button symbols with falling, weaving, looping, and drifting motion.
  private func drawFaceButtonField(
    context: inout GraphicsContext,
    size: CGSize,
    motionTime: TimeInterval,
    colorTime: TimeInterval
  ) {
    context.blendMode = .plusLighter

    let symbolCount = min(180, max(0, Int(72 * settings.faceButtonAmount)))
    for index in 0..<symbolCount {
      let color =
        index.isMultiple(of: 6)
        ? theme.highlightColor(index: index, time: colorTime)
        : theme.ribbonColor(index: index, time: colorTime)
      let center = symbolCenter(index: index, size: size, time: motionTime)
      let side = symbolSide(index: index, size: size, time: motionTime)
      let rotation = symbolRotation(index: index, time: motionTime)
      let opacity = (index.isMultiple(of: 5) ? 0.78 : 0.46) * settings.faceButtonOpacity

      drawFaceButtonSymbol(
        context: &context,
        kind: symbolKind(for: index),
        center: center,
        side: side,
        rotation: rotation,
        color: color,
        opacity: opacity
      )
    }
  }

  // Draws one PlayStation face-button outline with glow and a faint inner fill.
  private func drawFaceButtonSymbol(
    context: inout GraphicsContext,
    kind: FaceButtonSymbol,
    center: CGPoint,
    side: CGFloat,
    rotation: Double,
    color: Color,
    opacity: Double
  ) {
    let lineWidth = max(1.2, side * 0.07)
    let path: Path

    switch kind {
    case .square:
      path = polygonPath(
        sides: 4, center: center, radius: side * 0.56, rotation: rotation + .pi / 4)
    case .cross:
      path = crossPath(center: center, radius: side * 0.52, rotation: rotation)
    case .circle:
      path = Path(
        ellipseIn: CGRect(
          x: center.x - side * 0.45,
          y: center.y - side * 0.45,
          width: side * 0.9,
          height: side * 0.9
        )
      )
    case .triangle:
      path = polygonPath(sides: 3, center: center, radius: side * 0.6, rotation: rotation - .pi / 2)
    }

    if kind != .cross {
      context.fill(path, with: .color(color.opacity(opacity * 0.08)))
    }

    context.drawLayer { glow in
      glow.addFilter(.shadow(color: color.opacity(0.78), radius: 8))
      glow.stroke(
        path,
        with: .color(color.opacity(opacity)),
        style: StrokeStyle(lineWidth: lineWidth, lineCap: .round, lineJoin: .round)
      )
    }

    context.stroke(
      path,
      with: .color(.white.opacity(opacity * 0.24)),
      style: StrokeStyle(lineWidth: max(0.6, lineWidth * 0.42), lineCap: .round, lineJoin: .round)
    )
  }

  private func symbolKind(for index: Int) -> FaceButtonSymbol {
    switch index % 4 {
    case 0:
      return .square
    case 1:
      return .cross
    case 2:
      return .circle
    default:
      return .triangle
    }
  }

  // Chooses one of several deterministic motion styles for each symbol.
  private func symbolCenter(index: Int, size: CGSize, time: TimeInterval) -> CGPoint {
    let seedA = DynamicBackgroundMath.seededUnit(index: index, salt: 12.9898)
    let seedB = DynamicBackgroundMath.seededUnit(index: index, salt: 78.233)
    let seedC = DynamicBackgroundMath.seededUnit(index: index, salt: 37.719)
    let spread = settings.faceButtonDispersion
    let width = Double(size.width)
    let height = Double(size.height)

    switch index % 5 {
    case 0:
      let progress = DynamicBackgroundMath.unit(seedC + time * (0.025 + seedA * 0.03))
      let x =
        width * (0.08 + seedA * 0.84)
        + sin(time * 1.05 + seedB * 12) * width * 0.075 * spread
      let y = -height * 0.12 + progress * height * 1.24
      return CGPoint(x: CGFloat(x), y: CGFloat(y))
    case 1:
      let progress = DynamicBackgroundMath.unit(seedA + time * (0.012 + seedC * 0.016))
      let x =
        -width * 0.08
        + progress * width * 1.16
        + cos(time * 0.34 + seedB * 10) * width * 0.055 * spread
      let y =
        height * (0.18 + seedB * 0.64)
        + sin(progress * .pi * 3.2 + seedC * 8) * height * 0.08 * spread
      return CGPoint(x: CGFloat(x), y: CGFloat(y))
    case 2:
      let progress = DynamicBackgroundMath.unit(seedB + time * (0.018 + seedA * 0.014))
      let x = width * progress
      let y =
        height * (0.18 + seedC * 0.62)
        + sin(time * 0.9 + progress * .pi * 4 + seedA * 8) * height * 0.08 * spread
      return CGPoint(x: CGFloat(x), y: CGFloat(y))
    case 3:
      let radiusX = width * (0.14 + seedA * 0.22) * spread
      let radiusY = height * (0.11 + seedB * 0.18) * spread
      let x = width * 0.5 + sin(time * (0.22 + seedC * 0.16) + seedA * .pi * 2) * radiusX
      let y = height * 0.48 + cos(time * (0.28 + seedA * 0.16) + seedB * .pi * 2) * radiusY
      return CGPoint(x: CGFloat(x), y: CGFloat(y))
    default:
      let progress = DynamicBackgroundMath.unit(seedA - time * (0.02 + seedB * 0.018))
      let x =
        width * (0.12 + seedC * 0.76)
        + cos(time * 0.72 + seedA * 16) * width * 0.1 * spread
      let y = -height * 0.1 + progress * height * 1.22
      return CGPoint(x: CGFloat(x), y: CGFloat(y))
    }
  }

  private func symbolSide(index: Int, size: CGSize, time: TimeInterval) -> CGFloat {
    let seed = DynamicBackgroundMath.seededUnit(index: index, salt: 19.119)
    let base =
      min(size.width, size.height)
      * CGFloat(0.032 + seed * 0.042)
      * 0.5
      * CGFloat(settings.faceButtonSize)
    let pulse =
      1
      + CGFloat(sin(time * (0.65 + seed) + seed * 9))
      * 0.08
      * CGFloat(settings.faceButtonPulse)
    return min(58, max(8, base * pulse))
  }

  private func symbolRotation(index: Int, time: TimeInterval) -> Double {
    let seed = DynamicBackgroundMath.seededUnit(index: index, salt: 91.733)
    let direction = index.isMultiple(of: 2) ? 1.0 : -1.0
    return seed * .pi * 2 + time * (0.28 + seed * 0.46) * direction * settings.faceButtonRotation
  }

  private func polygonPath(
    sides: Int,
    center: CGPoint,
    radius: CGFloat,
    rotation: Double
  ) -> Path {
    var path = Path()

    for vertexIndex in 0..<sides {
      let angle = rotation + Double(vertexIndex) * (.pi * 2 / Double(sides))
      let point = CGPoint(
        x: center.x + CGFloat(cos(angle)) * radius,
        y: center.y + CGFloat(sin(angle)) * radius
      )

      if vertexIndex == 0 {
        path.move(to: point)
      } else {
        path.addLine(to: point)
      }
    }

    path.closeSubpath()
    return path
  }

  private func crossPath(
    center: CGPoint,
    radius: CGFloat,
    rotation: Double
  ) -> Path {
    var path = Path()
    let points = [
      (CGPoint(x: -radius, y: -radius), CGPoint(x: radius, y: radius)),
      (CGPoint(x: radius, y: -radius), CGPoint(x: -radius, y: radius)),
    ]

    for segment in points {
      path.move(to: rotatedPoint(segment.0, around: center, rotation: rotation))
      path.addLine(to: rotatedPoint(segment.1, around: center, rotation: rotation))
    }

    return path
  }

  private func rotatedPoint(
    _ offset: CGPoint,
    around center: CGPoint,
    rotation: Double
  ) -> CGPoint {
    CGPoint(
      x: center.x + offset.x * CGFloat(cos(rotation)) - offset.y * CGFloat(sin(rotation)),
      y: center.y + offset.x * CGFloat(sin(rotation)) + offset.y * CGFloat(cos(rotation))
    )
  }
}

private enum FaceButtonSymbol: Equatable {
  case square
  case cross
  case circle
  case triangle
}

// MARK: - PlayStationPortableBlurBackground

struct PlayStationPortableBlurBackground: View {
  let theme: DynamicBackgroundTheme

  var body: some View {
    TimelineView(.animation(minimumInterval: 1 / 30)) { timeline in
      let time =
        timeline.date.timeIntervalSinceReferenceDate
        * theme.particleSettings.backgrounds.playStationPortableBlurSpeed
      let settings = theme.particleSettings.backgrounds.playStationPortableBlur
      let base = theme.sharedColor(index: 1, time: time)
      let accent = theme.ribbonColor(index: 1, time: time)
      let shadow = theme.sharedColor(index: 0, time: time)
      let glow = theme.sharedColor(index: 2, time: time)
      let sharedGradient = theme.sharedGradientPoints(
        from: .topLeading,
        to: .bottomTrailing
      )

      ZStack {
        PaletteGradientField(
          colors: [
            theme.paletteBackgroundColor(
              shadow,
              darkness: 1 - min(1, settings.backgroundIntensity)
            ),
            theme.paletteBackgroundColor(
              base,
              darkness: 1 - min(1, settings.backgroundIntensity)
            ),
            theme.paletteBackgroundColor(
              accent,
              darkness: 1 - min(1, 0.74 * settings.backgroundIntensity)
            ),
            theme.paletteBackgroundColor(
              glow,
              darkness: 1 - min(1, 0.82 * settings.backgroundIntensity)
            ),
          ],
          startPoint: sharedGradient.start,
          endPoint: sharedGradient.end,
          curvature: theme.sharedPaletteGradientCurvature
        )

        RadialGradient(
          colors: [
            theme.highlightColor(index: 4, time: time)
              .opacity(0.34 * settings.ambientGlowIntensity),
            .clear,
          ],
          center: .center,
          startRadius: 10,
          endRadius: 430 * settings.ambientGlowScale
        )

        if settings.showsRibbons {
          Canvas { context, size in
            let ribbonCount = max(1, Int(settings.ribbonCount.rounded()))
            for index in 0..<ribbonCount {
              drawRibbon(
                context: &context,
                size: size,
                time: time,
                index: index
              )
            }
          }
        }

        LinearGradient(
          colors: [
            theme.paletteDarkOverlay(
              opacity: 0.12 * settings.vignetteIntensity
            ),
            .clear,
            theme.paletteDarkOverlay(
              opacity: 0.18 * settings.vignetteIntensity
            ),
          ],
          startPoint: .top,
          endPoint: .bottom
        )
      }
      .ignoresSafeArea()
    }
  }

  // Draws one XMB ribbon with a blurred glow and a thin bright core.
  private func drawRibbon(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    index: Int
  ) {
    let path = ribbonPath(size: size, time: time, index: index)
    let broadWidth = CGFloat(14 + (index % 4) * 7) * CGFloat(settings.broadWidth)
    let brightness = 0.14 + Double(index % 3) * 0.045
    let ribbonColor =
      index.isMultiple(of: 3)
      ? theme.ribbonColor(index: index, time: time)
      : theme.highlightColor(index: index, time: time)
    let coreColor = theme.highlightColor(index: index + 4, time: time)

    if settings.showsGlow {
      context.drawLayer { glow in
        glow.blendMode = .plusLighter
        glow.addFilter(
          .blur(
            radius: (9 + CGFloat(index % 3) * 4) * CGFloat(settings.glowBlur)
          )
        )
        glow.stroke(
          path,
          with: .color(ribbonColor.opacity(brightness * settings.glowOpacity)),
          style: StrokeStyle(
            lineWidth: broadWidth,
            lineCap: .round,
            lineJoin: .round
          )
        )
      }
    }

    if settings.showsCores {
      context.drawLayer { core in
        core.blendMode = .plusLighter
        core.addFilter(
          .blur(
            radius: (2.4 + CGFloat(index % 2)) * CGFloat(settings.coreBlur)
          )
        )
        core.stroke(
          path,
          with: .color(
            coreColor.opacity(
              (0.16 + Double(index % 3) * 0.035) * settings.coreOpacity
            )
          ),
          style: StrokeStyle(
            lineWidth: (2.2 + CGFloat(index % 3) * 0.7)
              * CGFloat(settings.coreWidth),
            lineCap: .round,
            lineJoin: .round
          )
        )
      }
    }
  }

  // Builds a smooth multi-point XMB wave path across the screen.
  private func ribbonPath(
    size: CGSize,
    time: TimeInterval,
    index: Int
  ) -> Path {
    let basePositions: [CGFloat] = [
      0.58, 0.62, 0.67, 0.7, 0.73, 0.76, 0.64,
    ]
    let amplitudes: [CGFloat] = [
      0.12, 0.095, 0.07, 0.045, 0.035, 0.055, 0.085,
    ]
    let profileIndex = index % basePositions.count
    let direction = settings.alternatesDirection && !index.isMultiple(of: 2) ? -1.0 : 1.0
    let phase =
      time * (0.12 + Double(index) * 0.011) * direction
      + Double(index) * 0.63 * settings.phaseSpread

    let points = (0...28).map { step in
      let progress = CGFloat(step) / 28
      let progressValue = Double(progress)
      let longWave = sin(progressValue * 2.15 + phase)
      let secondaryWave = sin(
        progressValue * 4.4 - phase * 0.52 + Double(index) * 0.37
      )
      let y =
        size.height * (basePositions[profileIndex] + CGFloat(settings.verticalOffset))
        + size.height * amplitudes[profileIndex] * CGFloat(settings.waveAmplitude)
        * CGFloat(longWave)
        + size.height * 0.018 * CGFloat(settings.detailAmplitude)
        * CGFloat(secondaryWave)

      return CGPoint(
        x: -size.width * 0.08 + progress * size.width * 1.16,
        y: y
      )
    }

    return DynamicBackgroundGeometry.smoothCurve(through: points)
  }

  private var settings: PlayStationPortableBlurSettings {
    theme.particleSettings.backgrounds.playStationPortableBlur
  }
}

// MARK: - PlayStation3SplinesBackground

enum PlayStationRibbonSizing {
  case playStation3
  case playStation4
}

struct PlayStation3SplinesBackground: View {
  let theme: DynamicBackgroundTheme
  let ribbonSizing: PlayStationRibbonSizing

  var body: some View {
    TimelineView(.animation(minimumInterval: 1 / 30)) { timeline in
      let time =
        timeline.date.timeIntervalSinceReferenceDate
        * theme.particleSettings.backgrounds.playStation3SplinesSpeed
      let settings = theme.particleSettings.backgrounds.playStation3Splines
      let dark = theme.sharedColor(index: 0, time: time)
      let primary = theme.sharedColor(index: 1, time: time)
      let ribbonPrimary = theme.ribbonColor(index: 1, time: time)
      let highlight = theme.ribbonColor(index: 2, time: time)
      let sharedGradient = theme.sharedGradientPoints(
        from: .topLeading,
        to: .bottomTrailing
      )

      ZStack {
        PaletteGradientField(
          colors: [
            theme.paletteBackgroundColor(
              dark,
              darkness: 1 - min(1, 0.58 * settings.backgroundIntensity)
            ),
            theme.paletteBackgroundColor(
              primary,
              darkness: 1 - min(1, 0.7 * settings.backgroundIntensity)
            ),
            theme.paletteBackgroundColor(
              primary,
              darkness: 1 - min(1, 0.46 * settings.backgroundIntensity)
            ),
            theme.paletteBackgroundColor(
              highlight,
              darkness: 1 - min(1, 0.24 * settings.backgroundIntensity)
            ),
          ],
          startPoint: sharedGradient.start,
          endPoint: sharedGradient.end,
          curvature: theme.sharedPaletteGradientCurvature
        )

        RadialGradient(
          colors: [
            highlight.opacity(0.16 * settings.backgroundIntensity),
            primary.opacity(0.08 * settings.backgroundIntensity),
            .clear,
          ],
          center: .bottomTrailing,
          startRadius: 8,
          endRadius: 520
        )

        Canvas { context, size in
          if settings.showsParticles {
            drawParticles(
              context: &context,
              size: size,
              time: time,
              color: highlight
            )
          }
          if settings.showsSplines {
            drawSplineBundle(
              context: &context,
              size: size,
              time: time,
              primary: ribbonPrimary,
              highlight: highlight
            )
          }
        }

        LinearGradient(
          colors: [
            theme.paletteDarkOverlay(
              opacity: 0.12 * settings.vignetteIntensity
            ),
            .clear,
            theme.paletteDarkOverlay(
              opacity: 0.18 * settings.vignetteIntensity
            ),
          ],
          startPoint: .top,
          endPoint: .bottom
        )
      }
      .ignoresSafeArea()
    }
  }

  // Draws the XMB3 ribbon bundle; XMB4 reuses it with PS4-sized stroke widths.
  private func drawSplineBundle(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    primary: Color,
    highlight: Color
  ) {
    let strandCount = max(1, Int(settings.strandCount.rounded()))

    for index in 0..<strandCount {
      let path = splinePath(size: size, time: time, index: index)
      let fade = 1 - Double(index) / Double(strandCount + 4)
      let color = index.isMultiple(of: 4) ? highlight : primary
      let widths = ribbonStrokeWidths(index: index)

      context.drawLayer { glow in
        glow.blendMode = .plusLighter
        glow.addFilter(
          .blur(radius: ribbonGlowBlur(index: index) * CGFloat(settings.glowBlur))
        )
        glow.stroke(
          path,
          with: .color(color.opacity(fade * 0.075 * settings.glowOpacity)),
          style: StrokeStyle(
            lineWidth: widths.glow * CGFloat(settings.glowWidth),
            lineCap: .round,
            lineJoin: .round
          )
        )
      }

      context.drawLayer { strand in
        strand.blendMode = .plusLighter
        strand.stroke(
          path,
          with: .color(
            index.isMultiple(of: 5)
              ? theme.highlightColor(index: index + 8, time: time)
                .opacity(fade * 0.58 * settings.coreOpacity)
              : color.opacity(fade * 0.3 * settings.coreOpacity)
          ),
          style: StrokeStyle(
            lineWidth: widths.core * CGFloat(settings.coreWidth),
            lineCap: .round,
            lineJoin: .round
          )
        )
      }
    }
  }

  // Draws small twinkling particles around the spline bundle.
  private func drawParticles(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    color: Color
  ) {
    context.blendMode = .plusLighter

    let particleCount = max(0, Int(settings.particleCount.rounded()))
    let particleTime = time * settings.particleSpeed

    for index in 0..<particleCount {
      let seed = DynamicBackgroundMath.unit(sin(Double(index + 1) * 12.9898) * 43_758.5453)
      let secondarySeed = DynamicBackgroundMath.unit(
        sin(Double(index + 7) * 78.233) * 9_631.417
      )
      let speed = 0.006 + Double(index % 7) * 0.0011
      let progress = DynamicBackgroundMath.unit(seed + particleTime * speed)
      let x = CGFloat(progress) * size.width
      let waveCenter =
        size.height
        * (CGFloat(settings.verticalPosition) + 0.02
          + 0.095
          * CGFloat(
            sin(progress * .pi * 2.1 - particleTime * 0.11 + seed * 5)
          ))
      let spread =
        (CGFloat(secondarySeed) - 0.5)
        * size.height
        * (0.18 + CGFloat(seed) * 0.2)
        * CGFloat(settings.particleSpread)
      let y =
        waveCenter + spread
        + CGFloat(sin(particleTime * 0.24 + seed * 18)) * 7
        * CGFloat(settings.particleSpread)
      let twinkle = pow(
        max(
          0,
          sin(particleTime * settings.particleTwinkle * (0.9 + seed) + secondarySeed * 14)
        ),
        3
      )
      let diameter =
        (CGFloat(0.7 + Double(index % 5) * 0.34)
          + CGFloat(twinkle) * 1.35) * CGFloat(settings.particleSize)
      let rect = CGRect(
        x: x - diameter / 2,
        y: y - diameter / 2,
        width: diameter,
        height: diameter
      )

      context.fill(
        Path(ellipseIn: rect),
        with: .color(
          index.isMultiple(of: 8)
            ? .white.opacity((0.35 + twinkle * 0.5) * settings.particleOpacity)
            : color.opacity((0.12 + twinkle * 0.28) * settings.particleOpacity)
        )
      )
    }
  }

  // Builds one animated XMB3 spline from sampled sine waves.
  private func splinePath(
    size: CGSize,
    time: TimeInterval,
    index: Int
  ) -> Path {
    let strand = Double(index)
    let direction = index.isMultiple(of: 3) ? -1.0 : 1.0
    let phase =
      time * (0.1 + strand * 0.0025) * direction
      + strand * 0.24 * settings.phaseSpread
    let centeredIndex =
      CGFloat(index) - CGFloat(max(0, Int(settings.strandCount.rounded()) - 1)) / 2
    let baseY =
      size.height
      * (CGFloat(settings.verticalPosition)
        + centeredIndex * 0.0095 * CGFloat(settings.strandSpacing))
    let amplitude =
      size.height
      * (0.09 + CGFloat(index % 4) * 0.008)
      * CGFloat(settings.waveAmplitude)

    let points = (0...32).map { step in
      let progress = CGFloat(step) / 32
      let progressValue = Double(progress)
      let primaryWave = sin(progressValue * .pi * 2.05 + phase)
      let detailWave = sin(
        progressValue * .pi * 4.1 - phase * 0.44 + strand * 0.33
      )
      let y =
        baseY
        + amplitude * CGFloat(primaryWave)
        + size.height * 0.018 * CGFloat(settings.detailAmplitude) * CGFloat(detailWave)

      return CGPoint(
        x: -size.width * 0.08 + progress * size.width * 1.16,
        y: y
      )
    }

    return DynamicBackgroundGeometry.smoothCurve(through: points)
  }

  // Chooses the glow and core widths for XMB3 or the wider XMB4 variant.
  private func ribbonStrokeWidths(index: Int) -> (glow: CGFloat, core: CGFloat) {
    switch ribbonSizing {
    case .playStation3:
      return (
        glow: 11 + CGFloat(index % 5) * 2.8,
        core: 0.55 + CGFloat(index % 4) * 0.34
      )
    case .playStation4:
      let waveIndex = index % 8
      return (
        glow: CGFloat(20 - waveIndex),
        core: CGFloat(0.7 + Double(7 - waveIndex) * 0.4)
      )
    }
  }

  // Matches the blur radius to the selected ribbon size profile.
  private func ribbonGlowBlur(index: Int) -> CGFloat {
    switch ribbonSizing {
    case .playStation3:
      return 10 + CGFloat(index % 4) * 2.5
    case .playStation4:
      return 18
    }
  }

  // Converts any floating value into a stable 0...1 seed.
  private var settings: PlayStation3SplinesSettings {
    theme.particleSettings.backgrounds.playStation3Splines
  }
}

// MARK: - PlayStation4ParticlesBackground

struct PlayStation4ParticlesBackground: View {
  let theme: DynamicBackgroundTheme

  var body: some View {
    TimelineView(.animation(minimumInterval: 1 / 30)) { timeline in
      let time =
        timeline.date.timeIntervalSinceReferenceDate
        * theme.particleSettings.backgrounds.playStation4ParticlesSpeed
      let settings = theme.particleSettings.backgrounds.playStation4Particles
      let dark = theme.sharedColor(index: 0, time: time)
      let primary = theme.sharedColor(index: 1, time: time)
      let ribbonPrimary = theme.ribbonColor(index: 1, time: time)
      let highlight = theme.ribbonColor(index: 2, time: time)
      let sharedGradient = theme.sharedGradientPoints(
        from: .topLeading,
        to: .bottomTrailing
      )

      ZStack {
        PaletteGradientField(
          colors: [
            theme.paletteBackgroundColor(
              dark,
              darkness: 1 - min(1, 0.58 * settings.backgroundIntensity)
            ),
            theme.paletteBackgroundColor(
              primary,
              darkness: 1 - min(1, 0.7 * settings.backgroundIntensity)
            ),
            theme.paletteBackgroundColor(
              primary,
              darkness: 1 - min(1, 0.46 * settings.backgroundIntensity)
            ),
            theme.paletteBackgroundColor(
              highlight,
              darkness: 1 - min(1, 0.24 * settings.backgroundIntensity)
            ),
          ],
          startPoint: sharedGradient.start,
          endPoint: sharedGradient.end,
          curvature: theme.sharedPaletteGradientCurvature
        )

        RadialGradient(
          colors: [
            highlight.opacity(0.16 * settings.backgroundIntensity),
            primary.opacity(0.08 * settings.backgroundIntensity),
            .clear,
          ],
          center: .bottomTrailing,
          startRadius: 8,
          endRadius: 520
        )

        Canvas { context, size in
          if settings.showsParticles {
            drawParticles(
              context: &context,
              size: size,
              time: time,
              color: highlight
            )
          }
          if settings.showsWaves {
            drawPlayStationWaves(
              context: &context,
              size: size,
              time: time,
              primary: ribbonPrimary,
              highlight: highlight
            )
          }
        }

        LinearGradient(
          colors: [
            theme.paletteDarkOverlay(
              opacity: 0.12 * settings.vignetteIntensity
            ),
            .clear,
            theme.paletteDarkOverlay(
              opacity: 0.18 * settings.vignetteIntensity
            ),
          ],
          startPoint: .top,
          endPoint: .bottom
        )
      }
      .ignoresSafeArea()
    }
  }

  // Draws the original XMB3 particles centered on the PS4 wave band.
  private func drawParticles(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    color: Color
  ) {
    context.blendMode = .plusLighter

    let particleCount = max(0, Int(settings.particleCount.rounded()))
    let particleTime = time * settings.particleSpeed

    for index in 0..<particleCount {
      let seed = DynamicBackgroundMath.unit(sin(Double(index + 1) * 12.9898) * 43_758.5453)
      let secondarySeed = DynamicBackgroundMath.unit(sin(Double(index + 7) * 78.233) * 9_631.417)
      let speed = 0.006 + Double(index % 7) * 0.0011
      let progress = DynamicBackgroundMath.unit(seed + particleTime * speed)
      let x = CGFloat(progress) * size.width
      let waveCenter =
        size.height
        * (CGFloat(settings.particleVerticalPosition)
          + 0.08
          * CGFloat(
            sin(progress * .pi * 2.1 - particleTime * 0.11 + seed * 5)
          ))
      let spread =
        (CGFloat(secondarySeed) - 0.5)
        * size.height
        * (0.12 + CGFloat(seed) * 0.16)
        * CGFloat(settings.particleSpread)
      let y =
        waveCenter + spread
        + CGFloat(sin(particleTime * 0.24 + seed * 18)) * 7
        * CGFloat(settings.particleSpread)
      let twinkle = pow(
        max(
          0,
          sin(particleTime * settings.particleTwinkle * (0.9 + seed) + secondarySeed * 14)
        ),
        3
      )
      let diameter =
        (CGFloat(0.7 + Double(index % 5) * 0.34)
          + CGFloat(twinkle) * 1.35) * CGFloat(settings.particleSize)
      let rect = CGRect(
        x: x - diameter / 2,
        y: y - diameter / 2,
        width: diameter,
        height: diameter
      )

      context.fill(
        Path(ellipseIn: rect),
        with: .color(
          index.isMultiple(of: 8)
            ? .white.opacity((0.35 + twinkle * 0.5) * settings.particleOpacity)
            : color.opacity((0.12 + twinkle * 0.28) * settings.particleOpacity)
        )
      )
    }
  }

  // Draws PS4 wave paths on top of the XMB3 background.
  private func drawPlayStationWaves(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    primary: Color,
    highlight: Color
  ) {
    let waveCount = max(1, Int(settings.waveCount.rounded()))

    for index in 0..<waveCount {
      let path = playStationWavePath(size: size, time: time, index: index)
      let fade = 1 - Double(index) / Double(waveCount + 2)

      if settings.showsWaveGlow {
        context.drawLayer { glow in
          glow.blendMode = .plusLighter
          glow.addFilter(.blur(radius: CGFloat(settings.glowBlur)))
          glow.stroke(
            path,
            with: .color(
              index.isMultiple(of: 2)
                ? primary.opacity(fade * 0.38 * settings.glowOpacity)
                : theme.ribbonColor(index: index + 1, time: time)
                  .opacity(fade * 0.32 * settings.glowOpacity)
            ),
            style: StrokeStyle(
              lineWidth: CGFloat(max(2, 20 - index)) * CGFloat(settings.glowWidth),
              lineCap: .round
            )
          )
        }
      }

      if settings.showsWaveCores {
        context.stroke(
          path,
          with: .color(
            index.isMultiple(of: 3)
              ? highlight.opacity(fade * 0.7 * settings.coreOpacity)
              : theme.ribbonColor(index: index + 2, time: time)
                .opacity(fade * 0.78 * settings.coreOpacity)
          ),
          style: StrokeStyle(
            lineWidth: CGFloat(0.7 + Double(waveCount - 1 - index) * 0.4)
              * CGFloat(settings.coreWidth),
            lineCap: .round
          )
        )
      }
    }
  }

  // Builds the same two-curve wave shape used by PS4 Waves.
  private func playStationWavePath(size: CGSize, time: TimeInterval, index: Int) -> Path {
    let layer = Double(index)
    let waveCount = max(1, Int(settings.waveCount.rounded()))
    let phase =
      time * (0.18 + layer * 0.012)
      + layer * 0.74 * settings.phaseSpread
    let centerIndex = CGFloat(max(0, waveCount - 2)) / 2
    let centerY =
      size.height
      * (CGFloat(settings.waveVerticalPosition)
        + (CGFloat(index) - centerIndex) * 0.033 * CGFloat(settings.waveSpacing))
    let amplitude =
      size.height * (0.055 + CGFloat(index % 3) * 0.014)
      * CGFloat(settings.waveAmplitude)
    let start = CGPoint(
      x: -size.width * 0.12,
      y: centerY + sin(phase) * amplitude
    )
    let midpoint = CGPoint(
      x: size.width * 0.5,
      y: centerY - cos(phase * 0.83) * amplitude
    )
    let end = CGPoint(
      x: size.width * 1.12,
      y: centerY + sin(phase * 0.71 + 1.4) * amplitude
    )
    let midpointTangentY = cos(phase * 0.68 + 0.35) * amplitude * 0.62

    var path = Path()
    path.move(to: start)
    path.addCurve(
      to: midpoint,
      control1: CGPoint(
        x: size.width * 0.08,
        y: centerY - cos(phase + 0.6) * amplitude * 1.5
          * CGFloat(settings.waveCurvature)
      ),
      control2: CGPoint(
        x: midpoint.x - size.width * 0.17,
        y: midpoint.y - midpointTangentY * CGFloat(settings.waveCurvature)
      )
    )
    path.addCurve(
      to: end,
      control1: CGPoint(
        x: midpoint.x + size.width * 0.17,
        y: midpoint.y + midpointTangentY * CGFloat(settings.waveCurvature)
      ),
      control2: CGPoint(
        x: size.width * 0.91,
        y: centerY + cos(phase + 0.2) * amplitude * 1.4
          * CGFloat(settings.waveCurvature)
      )
    )
    return path
  }
  private var settings: PlayStation4ParticlesSettings {
    theme.particleSettings.backgrounds.playStation4Particles
  }
}

// MARK: - PlayStation4WavesBackground

struct PlayStation4WavesBackground: View {
  let theme: DynamicBackgroundTheme
  var showsWaves = true

  var body: some View {
    TimelineView(.animation(minimumInterval: 1 / 30)) { timeline in
      let time =
        timeline.date.timeIntervalSinceReferenceDate
        * theme.particleSettings.backgrounds.playStation4WavesSpeed
      let settings = theme.particleSettings.backgrounds.playStation4Waves
      let backgroundDark = theme.playStation4BaseColor(index: 0, time: time)
      let backgroundMiddle = theme.playStation4BaseColor(index: 1, time: time)
      let backgroundBright = theme.playStation4BaseColor(index: 2, time: time)
      let highlight = theme.highlightColor(index: 9, time: time)
      let sharedGradient = theme.sharedGradientPoints(
        from: .topLeading,
        to: .bottomTrailing
      )

      ZStack {
        PaletteGradientField(
          colors: [
            backgroundDark,
            backgroundMiddle,
            theme.paletteBackgroundColor(backgroundBright, darkness: 0.54),
            theme.paletteBackgroundColor(backgroundDark, darkness: 0.22),
          ],
          startPoint: sharedGradient.start,
          endPoint: sharedGradient.end,
          curvature: theme.sharedPaletteGradientCurvature
        )

        if settings.showsAmbientGlows {
          Ellipse()
            .fill(
              theme.ribbonColor(index: 0, time: time)
                .opacity(0.3 * settings.ambientGlowIntensity)
            )
            .frame(
              width: 520 * settings.ambientGlowScale,
              height: 240 * settings.ambientGlowScale
            )
            .blur(radius: 90 * settings.ambientGlowScale)
            .offset(
              x: CGFloat(sin(time * 0.09)) * 180,
              y: CGFloat(cos(time * 0.07)) * 130 - 80
            )

          Circle()
            .fill(highlight.opacity(0.13 * settings.ambientGlowIntensity))
            .frame(
              width: 230 * settings.ambientGlowScale,
              height: 230 * settings.ambientGlowScale
            )
            .blur(radius: 78 * settings.ambientGlowScale)
            .offset(
              x: CGFloat(cos(time * 0.11)) * 220 + 120,
              y: CGFloat(sin(time * 0.08)) * 170
            )
        }

        if settings.showsParticles {
          Canvas { context, size in
            drawParticles(
              context: &context,
              size: size,
              time: time,
              highlight: highlight
            )
          }
        }

        if showsWaves && settings.showsWaves {
          Canvas { context, size in
            drawWaves(
              context: &context,
              size: size,
              time: time,
              highlight: highlight
            )
          }
        }

        LinearGradient(
          colors: [
            theme.paletteDarkOverlay(
              opacity: 0.18 * settings.vignetteIntensity
            ),
            .clear,
            theme.paletteDarkOverlay(
              opacity: 0.3 * settings.vignetteIntensity
            ),
          ],
          startPoint: .top,
          endPoint: .bottom
        )
      }
      .ignoresSafeArea()
    }
  }

  // Draws slow background particles colored by the ribbon palette.
  private func drawParticles(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    highlight: Color
  ) {
    let particleCount = max(0, Int(settings.particleCount.rounded()))
    let particleTime = time * settings.particleSpeed

    for index in 0..<particleCount {
      let seed = Double(index) * 1.618
      let x =
        (sin(particleTime * 0.035 + seed) * 0.5 + 0.5)
        * size.width
      let normalizedY =
        cos(particleTime * 0.026 + seed * 1.37) * 0.5
        * settings.particleSpread + 0.5
      let y = normalizedY * size.height
      let diameter =
        CGFloat(1.2 + Double(index % 4) * 0.55)
        * CGFloat(settings.particleSize)
      let particle = CGRect(
        x: x - diameter / 2,
        y: y - diameter / 2,
        width: diameter,
        height: diameter
      )
      context.fill(
        Path(ellipseIn: particle),
        with: .color(
          index.isMultiple(of: 5)
            ? highlight.opacity(0.5 * settings.particleOpacity)
            : theme.ribbonColor(index: index, time: time)
              .opacity(0.32 * settings.particleOpacity)
        )
      )
    }
  }

  // Draws the PS4 wave ribbons with a blurred glow and a sharp front stroke.
  private func drawWaves(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    highlight: Color
  ) {
    let waveCount = max(1, Int(settings.waveCount.rounded()))

    for index in 0..<waveCount {
      let path = wavePath(size: size, time: time, index: index)
      let fade = 1 - Double(index) / Double(waveCount + 2)

      if settings.showsWaveGlow {
        context.drawLayer { glow in
          glow.addFilter(.blur(radius: CGFloat(settings.glowBlur)))
          glow.stroke(
            path,
            with: .color(
              index.isMultiple(of: 2)
                ? theme.ribbonColor(index: index, time: time)
                  .opacity(fade * 0.38 * settings.glowOpacity)
                : theme.ribbonColor(index: index + 1, time: time)
                  .opacity(fade * 0.32 * settings.glowOpacity)
            ),
            style: StrokeStyle(
              lineWidth: CGFloat(max(2, 20 - index)) * CGFloat(settings.glowWidth),
              lineCap: .round
            )
          )
        }
      }

      if settings.showsWaveCores {
        context.stroke(
          path,
          with: .color(
            index.isMultiple(of: 3)
              ? highlight.opacity(fade * 0.7 * settings.coreOpacity)
              : theme.ribbonColor(index: index + 2, time: time)
                .opacity(fade * 0.78 * settings.coreOpacity)
          ),
          style: StrokeStyle(
            lineWidth: CGFloat(0.7 + Double(waveCount - 1 - index) * 0.4)
              * CGFloat(settings.coreWidth),
            lineCap: .round
          )
        )
      }
    }
  }

  // Builds one horizontal PS4 wave path from two cubic curves.
  private func wavePath(size: CGSize, time: TimeInterval, index: Int) -> Path {
    let layer = Double(index)
    let waveCount = max(1, Int(settings.waveCount.rounded()))
    let phase =
      time * (0.18 + layer * 0.012)
      + layer * 0.74 * settings.phaseSpread
    let centerIndex = CGFloat(max(0, waveCount - 2)) / 2
    let centerY =
      size.height
      * (CGFloat(settings.waveVerticalPosition)
        + (CGFloat(index) - centerIndex) * 0.033 * CGFloat(settings.waveSpacing))
    let amplitude =
      size.height * (0.055 + CGFloat(index % 3) * 0.014)
      * CGFloat(settings.waveAmplitude)
    let start = CGPoint(
      x: -size.width * 0.12,
      y: centerY + sin(phase) * amplitude
    )
    let midpoint = CGPoint(
      x: size.width * 0.5,
      y: centerY - cos(phase * 0.83) * amplitude
    )
    let end = CGPoint(
      x: size.width * 1.12,
      y: centerY + sin(phase * 0.71 + 1.4) * amplitude
    )
    let midpointTangentY = cos(phase * 0.68 + 0.35) * amplitude * 0.62

    var path = Path()
    path.move(to: start)
    path.addCurve(
      to: midpoint,
      control1: CGPoint(
        x: size.width * 0.08,
        y: centerY - cos(phase + 0.6) * amplitude * 1.5
          * CGFloat(settings.waveCurvature)
      ),
      control2: CGPoint(
        x: midpoint.x - size.width * 0.17,
        y: midpoint.y - midpointTangentY * CGFloat(settings.waveCurvature)
      )
    )
    path.addCurve(
      to: end,
      control1: CGPoint(
        x: midpoint.x + size.width * 0.17,
        y: midpoint.y + midpointTangentY * CGFloat(settings.waveCurvature)
      ),
      control2: CGPoint(
        x: size.width * 0.91,
        y: centerY + cos(phase + 0.2) * amplitude * 1.4
          * CGFloat(settings.waveCurvature)
      )
    )
    return path
  }

  private var settings: PlayStation4WavesSettings {
    theme.particleSettings.backgrounds.playStation4Waves
  }
}

// MARK: - PlayStationRibbonsBackground

struct PlayStationRibbonsBackground: View {
  let theme: DynamicBackgroundTheme

  var body: some View {
    TimelineView(.animation(minimumInterval: 1 / 30)) { timeline in
      let time =
        timeline.date.timeIntervalSinceReferenceDate
        * theme.particleSettings.backgrounds.playStationRibbonsSpeed
      let settings = theme.particleSettings.backgrounds.playStationRibbons
      let dark = theme.sharedColor(index: 0, time: time)
      let primary = theme.sharedColor(index: 1, time: time)
      let highlight = theme.ribbonColor(index: 2, time: time)
      let sharedGradient = theme.sharedGradientPoints(
        from: .topLeading,
        to: .bottomTrailing
      )

      ZStack {
        PaletteGradientField(
          colors: [
            theme.paletteBackgroundColor(dark, darkness: 0.96),
            theme.paletteBackgroundColor(dark, darkness: 0.54),
            theme.paletteBackgroundColor(primary, darkness: 0.76),
            theme.paletteBackgroundColor(primary, darkness: 1),
          ],
          startPoint: sharedGradient.start,
          endPoint: sharedGradient.end,
          curvature: theme.sharedPaletteGradientCurvature
        )

        Circle()
          .fill(primary.opacity(0.22 * settings.ambientGlowIntensity))
          .frame(
            width: 430 * settings.ambientGlowScale,
            height: 430 * settings.ambientGlowScale
          )
          .blur(radius: 110 * settings.ambientGlowScale)
          .offset(
            x: CGFloat(cos(time * 0.08)) * 170 + 90,
            y: CGFloat(sin(time * 0.06)) * 120 - 120
          )

        Canvas { context, size in
          if settings.showsPanels {
            drawLuminousPanels(
              context: &context,
              size: size,
              time: time
            )
          }
          if settings.showsParticles {
            drawFloatingLight(
              context: &context,
              size: size,
              time: time,
              color: highlight
            )
          }
        }

        LinearGradient(
          colors: [
            .white.opacity(0.035 * settings.vignetteIntensity),
            .clear,
            theme.paletteDarkOverlay(
              opacity: 0.28 * settings.vignetteIntensity
            ),
          ],
          startPoint: .top,
          endPoint: .bottom
        )
      }
      .ignoresSafeArea()
    }
  }
}

extension PlayStationRibbonsBackground {
  // Draws broad luminous panels whose fill color comes from the ribbon palette.
  private func drawLuminousPanels(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval
  ) {
    let panelCount = max(1, Int(settings.panelCount.rounded()))

    for index in 0..<panelCount {
      let panel = panelPath(size: size, time: time, index: index)
      let edge = panelEdgePath(size: size, time: time, index: index)
      let color = theme.ribbonColor(index: index + 1, time: time)
      let fade = 1 - Double(index) / Double(panelCount + 2)
      if settings.showsPanelGlow {
        context.drawLayer { glow in
          glow.addFilter(
            .blur(
              radius: (22 + CGFloat(index) * 3) * CGFloat(settings.panelGlowBlur)
            )
          )
          glow.fill(
            panel,
            with: .color(
              color.opacity(0.13 * fade * settings.panelGlowOpacity)
            )
          )
        }
      }

      context.fill(
        panel,
        with: .linearGradient(
          Gradient(colors: [
            .white.opacity(0.025 * settings.panelFillOpacity),
            color.opacity(0.2 * fade * settings.panelFillOpacity),
            .white.opacity(0.085 * fade * settings.panelFillOpacity),
          ]),
          startPoint: CGPoint(x: 0, y: size.height * 0.3),
          endPoint: CGPoint(x: size.width, y: size.height * 0.7)
        )
      )
      if settings.showsPanelEdges {
        context.stroke(
          edge,
          with: .color(
            .white.opacity(0.28 * fade * settings.panelEdgeOpacity)
          ),
          style: StrokeStyle(
            lineWidth: 0.8 * CGFloat(settings.panelEdgeWidth),
            lineCap: .round
          )
        )
      }
    }
  }

  // Draws slow floating points over the PS5 panels.
  private func drawFloatingLight(
    context: inout GraphicsContext,
    size: CGSize,
    time: TimeInterval,
    color: Color
  ) {
    let particleCount = max(0, Int(settings.particleCount.rounded()))
    let particleTime = time * settings.particleSpeed

    for index in 0..<particleCount {
      let seed = Double(index) * 1.618
      let progress =
        (Double(index) * 0.047
        + particleTime * (0.006 + Double(index % 5) * 0.0015))
        .truncatingRemainder(dividingBy: 1)
      let x =
        progress * size.width
        + CGFloat(sin(particleTime * 0.16 + seed)) * 24
        * CGFloat(settings.particleDrift)
      let normalizedY =
        cos(seed * 1.37) * 0.5
        * settings.particleVerticalSpread + 0.5
      let y =
        CGFloat(normalizedY) * size.height
        + CGFloat(sin(particleTime * 0.1 + seed)) * 38
        * CGFloat(settings.particleDrift)
      let diameter =
        CGFloat(0.8 + Double(index % 4) * 0.52)
        * CGFloat(settings.particleSize)
      let rect = CGRect(
        x: x - diameter / 2,
        y: y - diameter / 2,
        width: diameter,
        height: diameter
      )
      context.fill(
        Path(ellipseIn: rect),
        with: .color(
          index.isMultiple(of: 7)
            ? .white.opacity(0.5 * settings.particleOpacity)
            : color.opacity(0.25 * settings.particleOpacity)
        )
      )
    }
  }

  // Builds a thick curved panel from paired top and bottom edges.
  private func panelPath(
    size: CGSize,
    time: TimeInterval,
    index: Int
  ) -> Path {
    let phase =
      time * (0.12 + Double(index) * 0.006)
      + Double(index) * 0.72 * settings.phaseSpread
    let baseY =
      size.height
      * (CGFloat(settings.panelVerticalPosition)
        + CGFloat(index) * 0.135 * CGFloat(settings.panelSpacing))
    let amplitude =
      size.height * (0.055 + CGFloat(index % 2) * 0.018)
      * CGFloat(settings.panelAmplitude)
    let thickness =
      size.height * (0.04 + CGFloat(index % 3) * 0.012)
      * CGFloat(settings.panelThickness)
    let curvature = CGFloat(settings.panelCurvature)
    let startTop = CGPoint(
      x: -size.width * 0.18,
      y: baseY + CGFloat(sin(phase)) * amplitude
    )
    let endTop = CGPoint(
      x: size.width * 1.18,
      y: baseY + CGFloat(cos(phase * 0.79)) * amplitude
    )
    let startBottom = CGPoint(
      x: startTop.x,
      y: startTop.y + thickness
    )
    let endBottom = CGPoint(
      x: endTop.x,
      y: endTop.y + thickness * 0.7
    )

    var path = Path()
    path.move(to: startTop)
    path.addCurve(
      to: endTop,
      control1: CGPoint(
        x: size.width * 0.23,
        y: baseY - CGFloat(cos(phase + 0.4)) * amplitude * 1.8 * curvature
      ),
      control2: CGPoint(
        x: size.width * 0.76,
        y: baseY + CGFloat(sin(phase * 0.83 + 1.1)) * amplitude * 1.7 * curvature
      )
    )
    path.addLine(to: endBottom)
    path.addCurve(
      to: startBottom,
      control1: CGPoint(
        x: size.width * 0.76,
        y: baseY + thickness
          + CGFloat(sin(phase * 0.83 + 1.1)) * amplitude * 1.4
          * curvature
      ),
      control2: CGPoint(
        x: size.width * 0.23,
        y: baseY + thickness
          - CGFloat(cos(phase + 0.4)) * amplitude * 1.5
          * curvature
      )
    )
    path.closeSubpath()
    return path
  }

  // Builds the bright top edge that outlines a curved panel.
  private func panelEdgePath(
    size: CGSize,
    time: TimeInterval,
    index: Int
  ) -> Path {
    let phase =
      time * (0.12 + Double(index) * 0.006)
      + Double(index) * 0.72 * settings.phaseSpread
    let baseY =
      size.height
      * (CGFloat(settings.panelVerticalPosition)
        + CGFloat(index) * 0.135 * CGFloat(settings.panelSpacing))
    let amplitude =
      size.height * (0.055 + CGFloat(index % 2) * 0.018)
      * CGFloat(settings.panelAmplitude)
    let curvature = CGFloat(settings.panelCurvature)
    let start = CGPoint(
      x: -size.width * 0.18,
      y: baseY + CGFloat(sin(phase)) * amplitude
    )
    let end = CGPoint(
      x: size.width * 1.18,
      y: baseY + CGFloat(cos(phase * 0.79)) * amplitude
    )

    var path = Path()
    path.move(to: start)
    path.addCurve(
      to: end,
      control1: CGPoint(
        x: size.width * 0.23,
        y: baseY - CGFloat(cos(phase + 0.4)) * amplitude * 1.8 * curvature
      ),
      control2: CGPoint(
        x: size.width * 0.76,
        y: baseY + CGFloat(sin(phase * 0.83 + 1.1)) * amplitude * 1.7 * curvature
      )
    )
    return path
  }

  private var settings: PlayStationRibbonsSettings {
    theme.particleSettings.backgrounds.playStationRibbons
  }
}
