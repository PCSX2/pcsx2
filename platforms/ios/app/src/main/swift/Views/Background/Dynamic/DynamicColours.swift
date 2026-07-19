// DynamicColours.swift — Dynamic background colour and effects editor
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit

// MARK: - PaletteGradientField

struct PaletteGradientField: View {
  let colors: [Color]
  let startPoint: UnitPoint
  let endPoint: UnitPoint
  let curvature: Double

  var body: some View {
    if abs(curvature) < 0.001 {
      LinearGradient(
        colors: colors,
        startPoint: startPoint,
        endPoint: endPoint
      )
    } else {
      Canvas(opaque: false, colorMode: .linear) { context, size in
        drawCurvedGradient(context: &context, size: size)
      }
    }
  }

  private func drawCurvedGradient(
    context: inout GraphicsContext,
    size: CGSize
  ) {
    guard size.width > 0, size.height > 0, let firstColor = colors.first else {
      return
    }

    let resolvedColors = colors.map(PaletteGradientColor.init)
    let start = CGPoint(
      x: startPoint.x * size.width,
      y: startPoint.y * size.height
    )
    let end = CGPoint(
      x: endPoint.x * size.width,
      y: endPoint.y * size.height
    )
    let vector = CGVector(dx: end.x - start.x, dy: end.y - start.y)
    let length = max(1, hypot(vector.dx, vector.dy))
    let direction = CGVector(dx: vector.dx / length, dy: vector.dy / length)
    let perpendicular = CGVector(dx: -direction.dy, dy: direction.dx)
    let center = CGPoint(x: (start.x + end.x) * 0.5, y: (start.y + end.y) * 0.5)
    let diagonal = hypot(size.width, size.height)
    let clampedCurvature = CGFloat(min(1, max(-1, curvature)))
    let sampleCount = 28
    let bandCount = 144

    context.fill(
      Path(CGRect(origin: .zero, size: size)),
      with: .color(firstColor)
    )

    func boundary(_ progress: CGFloat) -> [CGPoint] {
      (0...sampleCount).map { sample in
        let crossProgress = CGFloat(sample) / CGFloat(sampleCount)
        let crossDistance = (crossProgress * 2 - 1) * diagonal
        let normalizedCrossDistance = crossDistance / diagonal
        let bend =
          clampedCurvature
          * normalizedCrossDistance * normalizedCrossDistance
          * length * 0.78
        let alongDistance = (progress - 0.5) * length - bend
        return CGPoint(
          x: center.x
            + direction.dx * alongDistance
            + perpendicular.dx * crossDistance,
          y: center.y
            + direction.dy * alongDistance
            + perpendicular.dy * crossDistance
        )
      }
    }

    let lastBoundary = boundary(1)
    var terminalPath = Path()
    if let first = lastBoundary.first {
      terminalPath.move(to: first)
      for point in lastBoundary.dropFirst() {
        terminalPath.addLine(to: point)
      }
      for point in lastBoundary.reversed() {
        terminalPath.addLine(
          to: CGPoint(
            x: point.x + direction.dx * diagonal * 4,
            y: point.y + direction.dy * diagonal * 4
          )
        )
      }
      terminalPath.closeSubpath()
      context.fill(
        terminalPath,
        with: .color(colors.last ?? firstColor)
      )
    }

    for band in 0..<bandCount {
      let lowerProgress = CGFloat(band) / CGFloat(bandCount)
      let upperProgress = CGFloat(band + 1) / CGFloat(bandCount)
      let lowerBoundary = boundary(lowerProgress)
      let upperBoundary = boundary(upperProgress + 0.001)
      guard let first = lowerBoundary.first else { continue }

      var bandPath = Path()
      bandPath.move(to: first)
      for point in lowerBoundary.dropFirst() {
        bandPath.addLine(to: point)
      }
      for point in upperBoundary.reversed() {
        bandPath.addLine(to: point)
      }
      bandPath.closeSubpath()

      let progress = Double(lowerProgress + upperProgress) * 0.5
      context.fill(
        bandPath,
        with: .color(
          PaletteGradientColor.interpolated(
            colors: resolvedColors,
            progress: progress
          ).color
        )
      )
    }
  }
}

private struct PaletteGradientColor {
  let red: CGFloat
  let green: CGFloat
  let blue: CGFloat
  let alpha: CGFloat

  init(_ color: Color) {
    let uiColor = UIColor(color)
    var red: CGFloat = 0
    var green: CGFloat = 0
    var blue: CGFloat = 0
    var alpha: CGFloat = 0
    if uiColor.getRed(&red, green: &green, blue: &blue, alpha: &alpha) {
      self.red = red
      self.green = green
      self.blue = blue
      self.alpha = alpha
    } else {
      self.red = 0
      self.green = 0
      self.blue = 0
      self.alpha = 0
    }
  }

  private init(red: CGFloat, green: CGFloat, blue: CGFloat, alpha: CGFloat) {
    self.red = red
    self.green = green
    self.blue = blue
    self.alpha = alpha
  }

  var color: Color {
    Color(red: red, green: green, blue: blue, opacity: alpha)
  }

  static func interpolated(
    colors: [PaletteGradientColor],
    progress: Double
  ) -> PaletteGradientColor {
    guard let first = colors.first else {
      return PaletteGradientColor(red: 0, green: 0, blue: 0, alpha: 0)
    }
    guard colors.count > 1 else { return first }

    let position = min(1, max(0, progress)) * Double(colors.count - 1)
    let lowerIndex = min(colors.count - 1, Int(floor(position)))
    let upperIndex = min(colors.count - 1, lowerIndex + 1)
    let amount = CGFloat(position - Double(lowerIndex))
    let lower = colors[lowerIndex]
    let upper = colors[upperIndex]
    return PaletteGradientColor(
      red: lower.red + (upper.red - lower.red) * amount,
      green: lower.green + (upper.green - lower.green) * amount,
      blue: lower.blue + (upper.blue - lower.blue) * amount,
      alpha: lower.alpha + (upper.alpha - lower.alpha) * amount
    )
  }
}

private struct PaletteGradientEditor: View {
  let title: String
  @Binding var colors: [Color]
  var isEnabled: Binding<Bool>?

  private let minimumStopCount = 2
  private let maximumStopCount = 8

  var body: some View {
    VStack(alignment: .leading, spacing: 10) {
      HStack(spacing: 10) {
        Text(title)
          .font(.subheadline.weight(.semibold))

        Spacer()

        if let isEnabled {
          Button {
            isEnabled.wrappedValue.toggle()
          } label: {
            Image(
              systemName: isEnabled.wrappedValue ? "eye.fill" : "eye.slash"
            )
          }
          .accessibilityLabel(
            isEnabled.wrappedValue ? "Disable \(title)" : "Enable \(title)"
          )
        }

        Button {
          resampleColors(to: colors.count - 1)
        } label: {
          Image(systemName: "minus")
        }
        .disabled(colors.count <= minimumStopCount)
        .accessibilityLabel("Remove \(title) gradient stop")

        Button {
          resampleColors(to: colors.count + 1)
        } label: {
          Image(systemName: "plus")
        }
        .disabled(colors.count >= maximumStopCount)
        .accessibilityLabel("Add \(title) gradient stop")
      }
      .buttonStyle(.borderless)

      GeometryReader { proxy in
        let handleDiameter: CGFloat = 40
        let trackWidth = max(0, proxy.size.width - handleDiameter)

        ZStack {
          RoundedRectangle(cornerRadius: 16, style: .continuous)
            .fill(
              LinearGradient(
                colors: colors,
                startPoint: .leading,
                endPoint: .trailing
              )
            )
            .frame(height: 32)
            .padding(.horizontal, handleDiameter / 2)

          ForEach(colors.indices, id: \.self) { index in
            ColorPicker(
              "\(title) stop \(index + 1)",
              selection: colorBinding(at: index),
              supportsOpacity: false
            )
            .labelsHidden()
            .frame(width: handleDiameter, height: handleDiameter)
            .background(Circle().fill(colors[index]))
            .overlay {
              Circle()
                .stroke(.black.opacity(0.85), lineWidth: 3)
                .allowsHitTesting(false)
            }
            .clipShape(Circle())
            .position(
              x: handleDiameter / 2
                + trackWidth * CGFloat(index) / CGFloat(colors.count - 1),
              y: proxy.size.height / 2
            )
          }
        }
        .opacity(isEnabled?.wrappedValue == false ? 0.35 : 1)
        .allowsHitTesting(isEnabled?.wrappedValue != false)
      }
      .frame(height: 46)
    }
    .padding(12)
    .glassSurface(tint: .white.opacity(0.06), cornerRadius: 14)
  }

  private func colorBinding(at index: Int) -> Binding<Color> {
    Binding(
      get: { colors[index] },
      set: { colors[index] = $0 }
    )
  }

  private func resampleColors(to count: Int) {
    guard minimumStopCount...maximumStopCount ~= count else { return }
    let resolvedColors = colors.map(PaletteGradientColor.init)
    colors = (0..<count).map { index in
      PaletteGradientColor.interpolated(
        colors: resolvedColors,
        progress: Double(index) / Double(count - 1)
      ).color
    }
  }
}

// MARK: - ThemePaletteControls

struct ThemePaletteControls: View {
  @Binding var target: ThemePaletteTarget
  @Binding var sharedPalette: ThemePalette
  @Binding var sharedCustomColor: SavedPaletteColor?
  @Binding var sharedMultiColor: ThemeMultiColorSelection
  @Binding var ribbonPalette: ThemePalette
  @Binding var ribbonCustomColor: SavedPaletteColor?
  @Binding var ribbonMultiColor: ThemeMultiColorSelection
  @Binding var particleSettings: DynamicParticleSettings
  @Binding var isPlayStation3XMBPresetExplicit: Bool
  @Binding var savedColorsJSON: String
  let dynamicBackground: DynamicBackgroundStyle
  let onSaveAppearance: () -> Void

  private let columns = Array(
    repeating: GridItem(.flexible(), spacing: 10),
    count: 4
  )

  @State private var draftColor = Color(red: 0.95, green: 0.2, blue: 0.82)
  @State private var draftPaletteColors = [
    Color(red: 0.08, green: 0.42, blue: 1),
    Color(red: 0.16, green: 0.88, blue: 0.92),
    Color(red: 0.74, green: 0.2, blue: 0.94),
  ]
  @State private var draftDarkEffectColors = [Color.black, Color.black]
  @State private var isDraftDarkEffectEnabled = true

  var body: some View {
    VStack(alignment: .leading, spacing: 20) {
      paletteSection
      customColorSection
      customPaletteSection
      multiColorControls
      paletteEffectsSection
    }
  }

  private var paletteSection: some View {
    let usesMultiColor = activeMultiColorSelection.wrappedValue.isEnabled

    return VStack(alignment: .leading, spacing: 10) {
      Label("Dynamic Palettes", systemImage: "sparkles")
        .font(.headline)

      paletteGrid(
        ThemePalette.primaryPalettes,
        usesMultiColor: usesMultiColor
      )

      Label("Plain Palettes", systemImage: "paintpalette.fill")
        .font(.headline)
        .padding(.top, 8)

      paletteGrid(
        ThemePalette.morePalettes,
        usesMultiColor: usesMultiColor
      )
    }
  }

  private func paletteGrid(
    _ palettes: [ThemePalette],
    usesMultiColor: Bool
  ) -> some View {
    LazyVGrid(columns: columns, spacing: 12) {
      ForEach(palettes) { palette in
        Button {
          if usesMultiColor {
            toggleMultiColorPalette(palette)
          } else {
            apply(palette)
          }
        } label: {
          VStack(spacing: 6) {
            RoundedRectangle(cornerRadius: 12, style: .continuous)
              .fill(
                LinearGradient(
                  colors: palettePreviewColors(for: palette),
                  startPoint: palettePreviewGradientPoints.start,
                  endPoint: palettePreviewGradientPoints.end
                )
              )
              .frame(height: 54)
              .overlay {
                paletteSelectionOverlay(
                  for: palette,
                  usesMultiColor: usesMultiColor
                )
              }

            Text(palette.title)
              .font(.caption2.weight(.semibold))
              .foregroundStyle(.white.opacity(0.86))
              .lineLimit(1)
              .minimumScaleFactor(0.7)
          }
        }
        .buttonStyle(.plain)
      }
    }
  }

  private var multiColorControls: some View {
    let selection = activeMultiColorSelection

    return VStack(alignment: .leading, spacing: 10) {
      Toggle("Multi-Color", isOn: multiColorEnabledBinding)

      Toggle("Animate", isOn: selection.animates)
        .disabled(!selection.wrappedValue.isEnabled)
        .opacity(selection.wrappedValue.isEnabled ? 1 : 0.48)
    }
    .font(.subheadline.weight(.semibold))
    .padding(12)
    .glassSurface(tint: .white.opacity(0.06), cornerRadius: 14)
  }

  @ViewBuilder
  private func paletteSelectionOverlay(
    for palette: ThemePalette,
    usesMultiColor: Bool
  ) -> some View {
    if usesMultiColor {
      if let number = multiColorNumber(for: palette) {
        Text("\(number)")
          .font(.caption.weight(.black))
          .foregroundStyle(.black)
          .frame(width: 24, height: 24)
          .background(Circle().fill(.white.opacity(0.94)))
          .shadow(radius: 4)
      }
    } else if isSelected(palette) {
      Image(systemName: "checkmark.circle.fill")
        .font(.title3)
        .foregroundStyle(.white)
        .shadow(radius: 4)
    }
  }

  private var customColorSection: some View {
    VStack(alignment: .leading, spacing: 12) {
      Text("Custom colours")
        .font(.headline)

      HStack(spacing: 12) {
        ColorPicker(
          "Choose a new colour",
          selection: $draftColor,
          supportsOpacity: false
        )

        Button {
          saveDraftColor()
        } label: {
          Label("Save", systemImage: "plus")
            .font(.subheadline.weight(.bold))
            .padding(.horizontal, 14)
            .frame(height: 36)
            .glassSurface(
              tint: draftColor.opacity(0.2),
              interactive: true,
              cornerRadius: 18
            )
        }
        .buttonStyle(.plain)
      }

      if !savedSolidColors.isEmpty {
        LazyVGrid(columns: columns, spacing: 10) {
          ForEach(savedSolidColors) { savedColor in
            Button {
              apply(savedColor)
            } label: {
              RoundedRectangle(cornerRadius: 11, style: .continuous)
                .fill(
                  LinearGradient(
                    colors: palettePreviewColors(for: savedColor),
                    startPoint: customColorPreviewGradientPoints.start,
                    endPoint: customColorPreviewGradientPoints.end
                  )
                )
                .frame(height: 52)
                .overlay {
                  if isSelected(savedColor) {
                    Image(systemName: "checkmark.circle.fill")
                      .foregroundStyle(.white)
                  }
                }
            }
            .buttonStyle(.plain)
            .contextMenu {
              Button(role: .destructive) {
                delete(savedColor)
              } label: {
                Label("Delete", systemImage: "trash")
              }
            }
            .accessibilityLabel("Saved colour \(savedColor.hex)")
            .accessibilityAction(named: "Delete") {
              delete(savedColor)
            }
          }
        }
      }
    }
  }

  private var customPaletteSection: some View {
    VStack(alignment: .leading, spacing: 12) {
      Text("Custom palette")
        .font(.headline)

      PaletteGradientEditor(
        title: "Palette gradient",
        colors: $draftPaletteColors
      )

      PaletteGradientEditor(
        title: "Dark effect",
        colors: $draftDarkEffectColors,
        isEnabled: $isDraftDarkEffectEnabled
      )

      HStack {
        Spacer()
        Button {
          saveDraftPalette()
        } label: {
          Label("Save palette", systemImage: "plus")
            .font(.subheadline.weight(.bold))
            .padding(.horizontal, 14)
            .frame(height: 36)
            .glassSurface(
              tint: draftPaletteColors[draftPaletteColors.count / 2].opacity(0.18),
              interactive: true,
              cornerRadius: 18
            )
        }
        .buttonStyle(.plain)
      }

      if !savedCustomPalettes.isEmpty {
        LazyVGrid(columns: columns, spacing: 10) {
          ForEach(savedCustomPalettes) { palette in
            Button {
              apply(palette)
            } label: {
              RoundedRectangle(cornerRadius: 11, style: .continuous)
                .fill(
                  LinearGradient(
                    colors: palettePreviewColors(for: palette),
                    startPoint: palettePreviewGradientPoints.start,
                    endPoint: palettePreviewGradientPoints.end
                  )
                )
                .frame(height: 52)
                .overlay {
                  if isSelected(palette) {
                    Image(systemName: "checkmark.circle.fill")
                      .foregroundStyle(.white)
                      .shadow(radius: 4)
                  }
                }
            }
            .buttonStyle(.plain)
            .contextMenu {
              Button(role: .destructive) {
                delete(palette)
              } label: {
                Label("Delete", systemImage: "trash")
              }
            }
            .accessibilityLabel("Saved custom palette")
            .accessibilityAction(named: "Delete") {
              delete(palette)
            }
          }
        }
      }
    }
  }

  private var paletteEffectsSection: some View {
    VStack(alignment: .leading, spacing: 12) {
      Toggle(
        "Disable dark effects on palettes",
        isOn: $particleSettings.disablesDarkPaletteEffects
      )
      .font(.subheadline.weight(.semibold))

      particleSlider(
        "Dark gradient strength",
        value: $particleSettings.paletteDarkEffectIntensity,
        range: 0...2,
        formattedValue: "\(Int(particleSettings.paletteDarkEffectIntensity * 100))%",
        resetValue: 1
      )
      .disabled(particleSettings.disablesDarkPaletteEffects)
      .opacity(particleSettings.disablesDarkPaletteEffects ? 0.45 : 1)

      if target == .shared {
        particleSlider(
          "Palette tilt gradient",
          value: $particleSettings.sharedPaletteGradientTilt,
          range: -180...180,
          formattedValue: gradientTiltDescription(
            particleSettings.sharedPaletteGradientTilt
          ),
          resetValue: 0
        )

        particleSlider(
          "Y-Axis gradient",
          value: $particleSettings.sharedPaletteGradientOffsetY,
          range: -1...1,
          formattedValue: gradientOffsetDescription(
            particleSettings.sharedPaletteGradientOffsetY
          ),
          resetValue: 0
        )

        particleSlider(
          "X-Axis gradient",
          value: $particleSettings.sharedPaletteGradientOffsetX,
          range: -1...1,
          formattedValue: gradientOffsetDescription(
            particleSettings.sharedPaletteGradientOffsetX
          ),
          resetValue: 0
        )

        particleSlider(
          "Gradient width",
          value: $particleSettings.sharedPaletteGradientWidth,
          range: 0.25...3,
          formattedValue: String(
            format: "%.2fx",
            particleSettings.sharedPaletteGradientWidth
          ),
          resetValue: 1
        )

        particleSlider(
          "Gradient curvature",
          value: $particleSettings.sharedPaletteGradientCurvature,
          range: -1...1,
          formattedValue: gradientOffsetDescription(
            particleSettings.sharedPaletteGradientCurvature
          ),
          resetValue: 0
        )
      }
    }
    .padding(12)
    .glassSurface(tint: .white.opacity(0.06), cornerRadius: 14)
  }

  private func palettePreviewColors(for palette: ThemePalette) -> [Color] {
    palette.colors
  }

  private func palettePreviewColors(for palette: SavedPaletteColor) -> [Color] {
    palette.gradientColors
  }

  private var activePaletteGradientTilt: Double {
    target == .shared ? particleSettings.sharedPaletteGradientTilt : 0
  }

  private var palettePreviewGradientPoints: (start: UnitPoint, end: UnitPoint) {
    rotatedPreviewGradientPoints(
      from: .topLeading,
      to: .bottomTrailing,
      degrees: activePaletteGradientTilt
    )
  }

  private var customColorPreviewGradientPoints: (start: UnitPoint, end: UnitPoint) {
    rotatedPreviewGradientPoints(
      from: .bottomLeading,
      to: .topTrailing,
      degrees: activePaletteGradientTilt
    )
  }

  private func gradientTiltDescription(_ degrees: Double) -> String {
    String(format: "%+.0f°", degrees)
  }

  private func gradientOffsetDescription(_ value: Double) -> String {
    String(format: "%+.0f%%", value * 100)
  }

  private func rotatedPreviewGradientPoints(
    from startPoint: UnitPoint,
    to endPoint: UnitPoint,
    degrees: Double
  ) -> (start: UnitPoint, end: UnitPoint) {
    let radians = degrees * .pi / 180
    let cosine = CGFloat(cos(radians))
    let sine = CGFloat(sin(radians))
    let appliesSharedGeometry = target == .shared
    let width =
      appliesSharedGeometry
      ? CGFloat(max(0.05, particleSettings.sharedPaletteGradientWidth))
      : 1
    let center = CGPoint(
      x: (startPoint.x + endPoint.x) * 0.5
        + (appliesSharedGeometry
          ? CGFloat(particleSettings.sharedPaletteGradientOffsetX) : 0),
      y: (startPoint.y + endPoint.y) * 0.5
        + (appliesSharedGeometry
          ? CGFloat(particleSettings.sharedPaletteGradientOffsetY) : 0)
    )

    func rotate(_ point: UnitPoint) -> UnitPoint {
      let sourceCenter = CGPoint(
        x: (startPoint.x + endPoint.x) * 0.5,
        y: (startPoint.y + endPoint.y) * 0.5
      )
      let x = (point.x - sourceCenter.x) * width
      let y = (point.y - sourceCenter.y) * width
      return UnitPoint(
        x: center.x + x * cosine - y * sine,
        y: center.y + x * sine + y * cosine
      )
    }

    return (rotate(startPoint), rotate(endPoint))
  }

  private func particleSlider(
    _ title: String,
    value: Binding<Double>,
    range: ClosedRange<Double>,
    formattedValue: String,
    resetValue: Double
  ) -> some View {
    DynamicSettingsValueSlider(
      title: title,
      value: value,
      range: range,
      step: nil,
      formattedValue: formattedValue,
      resetValue: resetValue
    )
  }

  // Decodes the saved custom colors each time the editor reads AppStorage.
  private var savedColors: [SavedPaletteColor] {
    DynamicBackgroundCoding.decode([SavedPaletteColor].self, from: savedColorsJSON) ?? []
  }

  private var savedSolidColors: [SavedPaletteColor] {
    savedColors.filter { !$0.isGradient }
  }

  private var savedCustomPalettes: [SavedPaletteColor] {
    savedColors.filter(\.isGradient)
  }

  private var activeMultiColorSelection: Binding<ThemeMultiColorSelection> {
    Binding(
      get: {
        switch target {
        case .shared:
          return sharedMultiColor
        case .ribbons:
          return ribbonMultiColor
        case .particles:
          return ThemeMultiColorSelection()
        }
      },
      set: { selection in
        switch target {
        case .shared:
          sharedMultiColor = selection
        case .ribbons:
          ribbonMultiColor = selection
        case .particles:
          break
        }
      }
    )
  }

  private var multiColorEnabledBinding: Binding<Bool> {
    Binding(
      get: {
        activeMultiColorSelection.wrappedValue.isEnabled
      },
      set: { enabled in
        setMultiColorEnabled(enabled)
      }
    )
  }

  private var isMultiColorEnabled: Bool {
    activeMultiColorSelection.wrappedValue.isEnabled
  }

  // Enables or disables multi-color mode for the active color target.
  private func setMultiColorEnabled(_ enabled: Bool) {
    switch target {
    case .shared:
      sharedMultiColor.setEnabled(enabled, fallback: sharedPalette)
      if enabled {
        sharedCustomColor = nil
        synchronizeRibbonPaletteAfterSharedSelection(sharedPalette)
      }
      useSharedThemeForXMBMart()
    case .ribbons:
      ribbonMultiColor.setEnabled(enabled, fallback: ribbonPalette)
      if enabled {
        ribbonCustomColor = nil
      }
      useSharedThemeForXMBMart()
    case .particles:
      break
    }
  }

  // Toggles a built-in palette in the active multi-color selection.
  private func toggleMultiColorPalette(_ palette: ThemePalette) {
    switch target {
    case .shared:
      sharedCustomColor = nil
      sharedMultiColor.toggle(palette, fallback: sharedPalette)
      synchronizeRibbonPaletteAfterSharedSelection(palette)
      useSharedThemeForXMBMart()
    case .ribbons:
      ribbonCustomColor = nil
      ribbonMultiColor.toggle(palette, fallback: ribbonPalette)
      useSharedThemeForXMBMart()
    case .particles:
      break
    }
  }

  // Returns the number displayed on selected multi-color palettes.
  private func multiColorNumber(for palette: ThemePalette) -> Int? {
    switch target {
    case .shared:
      return sharedMultiColor.selectionNumber(for: palette)
    case .ribbons:
      return ribbonMultiColor.selectionNumber(for: palette)
    case .particles:
      return nil
    }
  }

  // Applies a built-in palette to the currently selected target.
  private func apply(_ palette: ThemePalette) {
    switch target {
    case .shared:
      sharedMultiColor.isEnabled = false
      sharedPalette = palette
      sharedCustomColor = nil
      synchronizeRibbonPaletteAfterSharedSelection(palette)
      useSharedThemeForXMBMart()
    case .ribbons:
      ribbonMultiColor.isEnabled = false
      ribbonPalette = palette
      ribbonCustomColor = nil
      useSharedThemeForXMBMart()
    case .particles:
      break
    }
  }

  private func synchronizeRibbonPalette(with palette: ThemePalette) {
    ribbonMultiColor.isEnabled = false
    ribbonPalette = palette
    ribbonCustomColor = nil
  }

  private func synchronizeRibbonPaletteAfterSharedSelection(
    _ palette: ThemePalette
  ) {
    if dynamicBackground == .playStation3XMBByMart {
      synchronizeRibbonPalette(
        with: palette.isMorePalette ? palette.lightRibbonPaletteForMart : .cyan
      )
    } else if palette.isMorePalette {
      synchronizeRibbonPalette(with: palette)
    }
  }

  // Applies a saved custom color to the currently selected target.
  private func apply(_ color: SavedPaletteColor) {
    switch target {
    case .shared:
      sharedMultiColor.isEnabled = false
      sharedCustomColor = color
      if dynamicBackground == .playStation3XMBByMart, color.isGradient {
        synchronizeRibbonPalette(with: .cyan)
      }
      useSharedThemeForXMBMart()
    case .ribbons:
      ribbonMultiColor.isEnabled = false
      ribbonCustomColor = color
      useSharedThemeForXMBMart()
    case .particles:
      break
    }
  }

  private func useSharedThemeForXMBMart() {
    guard !isPlayStation3XMBPresetExplicit else { return }
    particleSettings.playStation3XMB.gradientPreset = .theme
  }

  // Checks whether a built-in palette is active for the selected target.
  private func isSelected(_ palette: ThemePalette) -> Bool {
    guard !isMultiColorEnabled else { return false }

    switch target {
    case .shared:
      return sharedCustomColor == nil && sharedPalette == palette
    case .ribbons:
      return ribbonCustomColor == nil && ribbonPalette == palette
    case .particles:
      return false
    }
  }

  // Checks whether a saved custom color is active for the selected target.
  private func isSelected(_ color: SavedPaletteColor) -> Bool {
    guard !isMultiColorEnabled else { return false }

    switch target {
    case .shared:
      return sharedCustomColor == color
    case .ribbons:
      return ribbonCustomColor == color
    case .particles:
      return false
    }
  }

  // Saves the draft color once and applies it immediately to the active target.
  private func saveDraftColor() {
    guard let color = SavedPaletteColor(color: draftColor) else { return }
    save(color)
    apply(color)
    onSaveAppearance()
  }

  private func saveDraftPalette() {
    guard
      let palette = SavedPaletteColor(
        colors: draftPaletteColors,
        darkEffectColors: isDraftDarkEffectEnabled ? draftDarkEffectColors : nil,
        darkEffectEnabled: isDraftDarkEffectEnabled
      )
    else {
      return
    }
    save(palette)
    apply(palette)
    onSaveAppearance()
  }

  private func save(_ color: SavedPaletteColor) {
    var colors = savedColors
    guard !colors.contains(color) else { return }
    colors.append(color)
    persistSavedColors(colors)
  }

  private func delete(_ color: SavedPaletteColor) {
    let colors = savedColors.filter { $0 != color }
    guard colors.count != savedColors.count else { return }

    if sharedCustomColor == color {
      sharedCustomColor = nil
    }
    if ribbonCustomColor == color {
      ribbonCustomColor = nil
    }

    persistSavedColors(colors)
  }

  private func persistSavedColors(_ colors: [SavedPaletteColor]) {
    guard let json = DynamicBackgroundCoding.encodeJSONString(colors) else { return }
    savedColorsJSON = json
  }
}

// MARK: - ThemePaletteEditorComponents

struct ThemePaletteEditorSnapshot: Equatable {
  let sharedPalette: ThemePalette
  let sharedCustomColor: SavedPaletteColor?
  let sharedMultiColor: ThemeMultiColorSelection
  let ribbonPalette: ThemePalette
  let ribbonCustomColor: SavedPaletteColor?
  let ribbonMultiColor: ThemeMultiColorSelection
  let particleSettings: DynamicParticleSettings
  let isPlayStation3XMBPresetExplicit: Bool
  let savedColorsJSON: String

  var visualState: ThemePaletteEditorVisualState {
    ThemePaletteEditorVisualState(
      sharedPalette: sharedPalette,
      sharedCustomColor: sharedCustomColor,
      sharedMultiColor: sharedMultiColor,
      ribbonPalette: ribbonPalette,
      ribbonCustomColor: ribbonCustomColor,
      ribbonMultiColor: ribbonMultiColor,
      particleSettings: particleSettings
    )
  }
}

struct ThemePaletteEditorVisualState: Equatable {
  let sharedPalette: ThemePalette
  let sharedCustomColor: SavedPaletteColor?
  let sharedMultiColor: ThemeMultiColorSelection
  let ribbonPalette: ThemePalette
  let ribbonCustomColor: SavedPaletteColor?
  let ribbonMultiColor: ThemeMultiColorSelection
  let particleSettings: DynamicParticleSettings
}

struct ThemePaletteEditorHeader: View {
  let canUndo: Bool
  let canRedo: Bool
  let cancel: () -> Void
  let undo: () -> Void
  let redo: () -> Void
  let apply: () -> Void

  var body: some View {
    HStack(spacing: 10) {
      Text("Colours")
        .font(.title2.weight(.bold))

      Spacer(minLength: 8)

      HStack(spacing: 2) {
        Button("Cancel", action: cancel)
          .padding(.horizontal, 7)

        Button(action: undo) {
          Image(systemName: "arrow.uturn.backward")
            .frame(width: 28, height: 32)
        }
        .disabled(!canUndo)
        .accessibilityLabel("Undo")
        .help("Undo")

        Button(action: redo) {
          Image(systemName: "arrow.uturn.forward")
            .frame(width: 28, height: 32)
        }
        .disabled(!canRedo)
        .accessibilityLabel("Redo")
        .help("Redo")

        Button("Apply", action: apply)
          .fontWeight(.semibold)
          .padding(.horizontal, 7)
      }
      .font(.subheadline.weight(.medium))
      .padding(.horizontal, 5)
      .frame(height: 42)
      .glassSurface(interactive: true, cornerRadius: 21)
      .buttonStyle(.plain)
    }
  }
}

struct ThemePaletteEditorSliderReadout: View {
  let title: String
  let value: String

  var body: some View {
    VStack {
      HStack {
        Spacer()
        HStack(spacing: 8) {
          Text(title)
            .foregroundStyle(.white.opacity(0.72))
          Text(value)
            .foregroundStyle(.white)
            .monospacedDigit()
        }
        .font(.caption.weight(.semibold))
        .lineLimit(1)
        .padding(.horizontal, 14)
        .frame(height: 36)
        .glassSurface(cornerRadius: 18)
      }
      Spacer()
    }
    .padding(18)
    .allowsHitTesting(false)
    .accessibilityElement(children: .combine)
  }
}

// MARK: - DynamicSettingsEditor

struct ThemePaletteEditor: View {
  @Environment(\.dismiss) private var dismiss

  @Binding var target: ThemePaletteTarget
  @Binding var sharedPalette: ThemePalette
  @Binding var sharedCustomColor: SavedPaletteColor?
  @Binding var sharedMultiColor: ThemeMultiColorSelection
  @Binding var ribbonPalette: ThemePalette
  @Binding var ribbonCustomColor: SavedPaletteColor?
  @Binding var ribbonMultiColor: ThemeMultiColorSelection
  @Binding var particleSettings: DynamicParticleSettings
  @Binding var isShowingBackgroundOnly: Bool
  @Binding var isPlayStation3XMBPresetExplicit: Bool
  let dynamicBackground: DynamicBackgroundStyle
  let onSaveAppearance: () -> Void

  @AppStorage("ARMSX2iOSDynamicSavedPaletteColors") private var savedColorsJSON = "[]"
  @State private var previewDebounceTask: Task<Void, Never>?
  @State private var backgroundPreviewTask: Task<Void, Never>?
  @State private var previewFadeTask: Task<Void, Never>?
  @State private var backgroundPreviewOpacity = 0.0
  @State private var lastThemeSnapshot: DynamicBackgroundTheme?
  @State private var pendingOriginalTheme: DynamicBackgroundTheme?
  @State private var pendingUpdatedTheme: DynamicBackgroundTheme?
  @State private var activeUpdatedTheme: DynamicBackgroundTheme?
  @State private var previewThemeSnapshot: DynamicBackgroundTheme?
  @State private var previewShowsUpdatedTheme = false
  @State private var activeSliderTitle: String?
  @State private var activeSliderValue: String?
  @State private var lastSliderInteractionEnd = Date.distantPast
  @State private var tabTransitionMovesForward = true
  @State private var scrollToTopRequest = 0
  @State private var undoSnapshots: [ThemePaletteEditorSnapshot] = []
  @State private var redoSnapshots: [ThemePaletteEditorSnapshot] = []
  @State private var initialEditorSnapshot: ThemePaletteEditorSnapshot?
  @State private var lastEditorSnapshot: ThemePaletteEditorSnapshot?
  @State private var pendingUndoSnapshot: ThemePaletteEditorSnapshot?
  @State private var historyCommitTask: Task<Void, Never>?
  @State private var isClosingEditor = false

  init(
    target: Binding<ThemePaletteTarget>,
    preferences: Binding<DynamicAppearancePreferences>,
    isShowingBackgroundOnly: Binding<Bool>,
    dynamicBackground: DynamicBackgroundStyle,
    onSaveAppearance: @escaping () -> Void
  ) {
    _target = target
    _sharedPalette = preferences.sharedPalette
    _sharedCustomColor = preferences.sharedCustomColor
    _sharedMultiColor = preferences.sharedMultiColor
    _ribbonPalette = preferences.ribbonPalette
    _ribbonCustomColor = preferences.ribbonCustomColor
    _ribbonMultiColor = preferences.ribbonMultiColor
    _particleSettings = preferences.particleSettings
    _isShowingBackgroundOnly = isShowingBackgroundOnly
    _isPlayStation3XMBPresetExplicit = preferences.isPlayStation3XMBPresetExplicit
    self.dynamicBackground = dynamicBackground
    self.onSaveAppearance = onSaveAppearance
  }

  var body: some View {
    editorSurface
      .preferredColorScheme(.dark)
      .presentationBackground(.clear)
      .presentationDragIndicator(isShowingBackgroundOnly ? .hidden : .visible)
      .onAppear(perform: prepareEditor)
      .onChange(of: currentEditorSnapshot) { oldSnapshot, newSnapshot in
        handleEditorSnapshotChange(from: oldSnapshot, to: newSnapshot)
      }
      .onDisappear(perform: tearDownEditor)
  }

  // The explicit erasure keeps the release runtime from decoding the entire sheet's
  // deeply nested SwiftUI type when the presentation bridge creates its host.
  private var editorSurface: AnyView {
    AnyView(
      editorNavigation
        .overlay {
          if isShowingBackgroundOnly {
            backgroundPreview
              .opacity(backgroundPreviewOpacity)
          }
        }
        .overlay {
          if isShowingBackgroundOnly {
            if let activeSliderTitle,
              let activeSliderValue
            {
              ThemePaletteEditorSliderReadout(
                title: activeSliderTitle,
                value: activeSliderValue
              )
              .opacity(backgroundPreviewOpacity)
            }
          }
        }
    )
  }

  private var editorNavigation: AnyView {
    AnyView(
      NavigationStack {
        editorScroll
      }
      .simultaneousGesture(tabSwipeGesture)
      .opacity(1 - backgroundPreviewOpacity)
      .allowsHitTesting(!isShowingBackgroundOnly || activeSliderTitle != nil)
      .environment(
        \.dynamicSettingsSliderActivity,
        DynamicSettingsSliderActivity(update: updateSliderActivity)
      )
    )
  }

  private var editorScroll: AnyView {
    AnyView(
      ScrollViewReader { proxy in
        ScrollView {
          editorSections
        }
        .onChange(of: scrollToTopRequest) { _, _ in
          var transaction = Transaction(animation: nil)
          transaction.disablesAnimations = true
          withTransaction(transaction) {
            proxy.scrollTo("colours-editor-top", anchor: .top)
          }
        }
      }
      .background(
        Color.clear
          .glassSurface(cornerRadius: 0)
          .ignoresSafeArea()
      )
    )
  }

  private var editorSections: AnyView {
    AnyView(
      VStack(alignment: .leading, spacing: 20) {
        ThemePaletteEditorHeader(
          canUndo: canRevert,
          canRedo: !redoSnapshots.isEmpty,
          cancel: cancelChanges,
          undo: revertLastChange,
          redo: redoLastChange,
          apply: applyChanges
        )
        .id("colours-editor-top")

        Picker("Color target", selection: $target) {
          ForEach(ThemePaletteTarget.allCases) { target in
            Text(target.title).tag(target)
          }
        }
        .pickerStyle(.segmented)

        selectedTabSections
          .id(target)
          .transition(tabTransition)
      }
      .padding(18)
      .clipped()
    )
  }

  @ViewBuilder
  private var selectedTabSections: some View {
    if target == .particles {
      DynamicParticleSettingsControls(
        particleSettings: $particleSettings,
        isPlayStation3XMBPresetExplicit: $isPlayStation3XMBPresetExplicit,
        dynamicBackground: dynamicBackground,
        resetAllSettingsAndPalettes: resetDynamicBackgroundSettingsAndPalettes
      )
    } else {
      ThemePaletteControls(
        target: $target,
        sharedPalette: $sharedPalette,
        sharedCustomColor: $sharedCustomColor,
        sharedMultiColor: $sharedMultiColor,
        ribbonPalette: $ribbonPalette,
        ribbonCustomColor: $ribbonCustomColor,
        ribbonMultiColor: $ribbonMultiColor,
        particleSettings: $particleSettings,
        isPlayStation3XMBPresetExplicit: $isPlayStation3XMBPresetExplicit,
        savedColorsJSON: $savedColorsJSON,
        dynamicBackground: dynamicBackground,
        onSaveAppearance: onSaveAppearance
      )
    }
  }

  private var tabTransition: AnyTransition {
    let insertionEdge: Edge = tabTransitionMovesForward ? .trailing : .leading
    let removalEdge: Edge = tabTransitionMovesForward ? .leading : .trailing
    return .asymmetric(
      insertion: .move(edge: insertionEdge).combined(with: .opacity),
      removal: .move(edge: removalEdge).combined(with: .opacity)
    )
  }

  private var tabSwipeGesture: some Gesture {
    DragGesture(minimumDistance: 28)
      .onEnded(handleTabSwipe)
  }

  private func prepareEditor() {
    initialEditorSnapshot = currentEditorSnapshot
    lastThemeSnapshot = currentTheme
    lastEditorSnapshot = currentEditorSnapshot
  }

  private func handleTabSwipe(_ value: DragGesture.Value) {
    guard
      !isShowingBackgroundOnly,
      activeSliderTitle == nil,
      Date().timeIntervalSince(lastSliderInteractionEnd) > 0.2
    else {
      return
    }

    let horizontalDistance = value.translation.width
    let verticalDistance = value.translation.height
    guard
      abs(horizontalDistance) >= 64,
      abs(horizontalDistance) > abs(verticalDistance) * 1.25
    else {
      return
    }

    moveToAdjacentTab(forward: horizontalDistance < 0)
  }

  private func moveToAdjacentTab(forward: Bool) {
    let tabs = ThemePaletteTarget.allCases
    guard let currentIndex = tabs.firstIndex(of: target) else { return }
    let nextIndex = currentIndex + (forward ? 1 : -1)
    guard tabs.indices.contains(nextIndex) else { return }

    tabTransitionMovesForward = forward
    withAnimation(.easeInOut(duration: 0.28)) {
      target = tabs[nextIndex]
      scrollToTopRequest += 1
    }
  }

  private var backgroundPreview: AnyView {
    AnyView(
      ZStack {
        previewBackgroundLayer
        previewDismissLayer
      }
    )
  }

  private var previewBackgroundLayer: AnyView {
    AnyView(
      DynamicBackgroundContentView(
        style: dynamicBackground,
        theme: previewTheme
      )
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .ignoresSafeArea()
        .clipped()
        .allowsHitTesting(false)
        .accessibilityHidden(true)
    )
  }

  private var previewDismissLayer: AnyView {
    AnyView(
      Color.clear
        .contentShape(Rectangle())
        .ignoresSafeArea()
        .allowsHitTesting(activeSliderTitle == nil)
        .onTapGesture(perform: endBackgroundPreview)
        .accessibilityLabel("Show controls")
    )
  }

  private var currentTheme: DynamicBackgroundTheme {
    DynamicBackgroundTheme(
      sharedPalette: sharedPalette,
      sharedCustomColor: sharedCustomColor,
      sharedMultiColor: sharedMultiColor,
      ribbonPalette: ribbonPalette,
      ribbonCustomColor: ribbonCustomColor,
      ribbonMultiColor: ribbonMultiColor,
      particleSettings: particleSettings
    )
  }

  private var previewTheme: DynamicBackgroundTheme {
    previewThemeSnapshot ?? currentTheme
  }

  private func scheduleBackgroundPreview() {
    previewDebounceTask?.cancel()

    let beforeDuration = particleSettings.backgroundPreviewBeforeApplyingDuration
    let afterDuration = particleSettings.backgroundPreviewAfterApplyingDuration
    guard beforeDuration > 0 || afterDuration > 0 else {
      endBackgroundPreview()
      lastThemeSnapshot = currentTheme
      return
    }

    let updatedTheme = currentTheme
    let previousTheme = lastThemeSnapshot ?? updatedTheme
    lastThemeSnapshot = updatedTheme

    if isShowingBackgroundOnly {
      previewFadeTask?.cancel()
      activeUpdatedTheme = updatedTheme
      pendingUpdatedTheme = updatedTheme

      if backgroundPreviewOpacity < 1 {
        withAnimation(.easeInOut(duration: 0.1)) {
          backgroundPreviewOpacity = 1
        }
      }

      if previewShowsUpdatedTheme {
        var themeTransaction = Transaction(animation: nil)
        themeTransaction.disablesAnimations = true
        withTransaction(themeTransaction) {
          previewThemeSnapshot = updatedTheme
        }
        restartBackgroundPreviewHold(afterDuration: afterDuration)
      }
      return
    }

    backgroundPreviewTask?.cancel()
    if pendingOriginalTheme == nil {
      pendingOriginalTheme = previousTheme
    }
    pendingUpdatedTheme = updatedTheme

    previewDebounceTask = Task { @MainActor in
      await Task.yield()
      guard !Task.isCancelled else { return }
      beginBackgroundPreview(
        beforeDuration: beforeDuration,
        afterDuration: afterDuration
      )
    }
  }

  private func beginBackgroundPreview(
    beforeDuration: Double,
    afterDuration: Double
  ) {
    let originalTheme = pendingOriginalTheme ?? lastThemeSnapshot ?? currentTheme
    previewFadeTask?.cancel()
    activeUpdatedTheme = pendingUpdatedTheme ?? currentTheme
    pendingOriginalTheme = nil
    pendingUpdatedTheme = nil
    previewShowsUpdatedTheme = beforeDuration <= 0
    previewThemeSnapshot =
      beforeDuration > 0
      ? originalTheme
      : activeUpdatedTheme ?? currentTheme

    backgroundPreviewTask?.cancel()
    var visibilityTransaction = Transaction(animation: nil)
    visibilityTransaction.disablesAnimations = true
    withTransaction(visibilityTransaction) {
      backgroundPreviewOpacity = 0
      isShowingBackgroundOnly = true
    }
    withAnimation(.easeInOut(duration: 0.1)) {
      backgroundPreviewOpacity = 1
    }

    backgroundPreviewTask = Task { @MainActor in
      if beforeDuration > 0 {
        try? await Task.sleep(for: .seconds(beforeDuration))
        guard !Task.isCancelled else { return }

        var themeTransaction = Transaction(animation: nil)
        themeTransaction.disablesAnimations = true
        withTransaction(themeTransaction) {
          previewShowsUpdatedTheme = true
          previewThemeSnapshot = activeUpdatedTheme ?? currentTheme
        }
      }

      guard activeSliderTitle == nil else { return }
      if afterDuration > 0 {
        try? await Task.sleep(for: .seconds(afterDuration))
        guard !Task.isCancelled else { return }
      }
      endBackgroundPreview()
    }
  }

  private func restartBackgroundPreviewHold(afterDuration: Double) {
    backgroundPreviewTask?.cancel()
    guard activeSliderTitle == nil else { return }

    backgroundPreviewTask = Task { @MainActor in
      if afterDuration > 0 {
        try? await Task.sleep(for: .seconds(afterDuration))
        guard !Task.isCancelled else { return }
      }
      endBackgroundPreview()
    }
  }

  private func updateSliderActivity(
    title: String,
    value: String,
    isEditing: Bool
  ) {
    if isEditing {
      let beginsInteraction = activeSliderTitle == nil
      activeSliderTitle = title
      activeSliderValue = value
      if beginsInteraction {
        beginSliderBackgroundPreviewIfNeeded()
      }
      return
    }

    guard activeSliderTitle == title else { return }
    activeSliderTitle = nil
    activeSliderValue = nil
    lastSliderInteractionEnd = Date()

    if isShowingBackgroundOnly, previewShowsUpdatedTheme {
      restartBackgroundPreviewHold(
        afterDuration: particleSettings.backgroundPreviewAfterApplyingDuration
      )
    }
  }

  private func beginSliderBackgroundPreviewIfNeeded() {
    guard !isShowingBackgroundOnly else { return }

    let beforeDuration = particleSettings.backgroundPreviewBeforeApplyingDuration
    let afterDuration = particleSettings.backgroundPreviewAfterApplyingDuration
    guard beforeDuration > 0 || afterDuration > 0 else { return }

    previewDebounceTask?.cancel()
    backgroundPreviewTask?.cancel()
    let unchangedTheme = lastThemeSnapshot ?? currentTheme
    pendingOriginalTheme = unchangedTheme
    pendingUpdatedTheme = currentTheme
    beginBackgroundPreview(
      beforeDuration: beforeDuration,
      afterDuration: afterDuration
    )
  }

  private func endBackgroundPreview() {
    previewDebounceTask?.cancel()
    backgroundPreviewTask?.cancel()
    previewFadeTask?.cancel()
    pendingOriginalTheme = nil
    pendingUpdatedTheme = nil
    withAnimation(.easeInOut(duration: 0.2)) {
      backgroundPreviewOpacity = 0
    }

    previewFadeTask = Task { @MainActor in
      try? await Task.sleep(for: .milliseconds(100))
      guard !Task.isCancelled, backgroundPreviewOpacity == 0 else { return }

      activeUpdatedTheme = nil
      previewThemeSnapshot = nil
      previewShowsUpdatedTheme = false
      var visibilityTransaction = Transaction(animation: nil)
      visibilityTransaction.disablesAnimations = true
      withTransaction(visibilityTransaction) {
        isShowingBackgroundOnly = false
      }
    }
  }

  private var currentEditorSnapshot: ThemePaletteEditorSnapshot {
    ThemePaletteEditorSnapshot(
      sharedPalette: sharedPalette,
      sharedCustomColor: sharedCustomColor,
      sharedMultiColor: sharedMultiColor,
      ribbonPalette: ribbonPalette,
      ribbonCustomColor: ribbonCustomColor,
      ribbonMultiColor: ribbonMultiColor,
      particleSettings: particleSettings,
      isPlayStation3XMBPresetExplicit: isPlayStation3XMBPresetExplicit,
      savedColorsJSON: savedColorsJSON
    )
  }

  private var canRevert: Bool {
    pendingUndoSnapshot != nil || !undoSnapshots.isEmpty
  }

  private func handleEditorSnapshotChange(
    from oldSnapshot: ThemePaletteEditorSnapshot,
    to newSnapshot: ThemePaletteEditorSnapshot
  ) {
    guard !isClosingEditor else { return }
    recordEditorChange()
    guard oldSnapshot.visualState != newSnapshot.visualState else { return }

    if isOnlyPortraitWideningChange(from: oldSnapshot, to: newSnapshot)
      || isOnlyBackgroundPreviewTimingChange(from: oldSnapshot, to: newSnapshot)
    {
      lastThemeSnapshot = currentTheme
      return
    }

    scheduleBackgroundPreview()
  }

  private func isOnlyPortraitWideningChange(
    from oldSnapshot: ThemePaletteEditorSnapshot,
    to newSnapshot: ThemePaletteEditorSnapshot
  ) -> Bool {
    guard
      oldSnapshot.sharedPalette == newSnapshot.sharedPalette,
      oldSnapshot.sharedCustomColor == newSnapshot.sharedCustomColor,
      oldSnapshot.sharedMultiColor == newSnapshot.sharedMultiColor,
      oldSnapshot.ribbonPalette == newSnapshot.ribbonPalette,
      oldSnapshot.ribbonCustomColor == newSnapshot.ribbonCustomColor,
      oldSnapshot.ribbonMultiColor == newSnapshot.ribbonMultiColor,
      oldSnapshot.particleSettings.widensPortraitBackground
        != newSnapshot.particleSettings.widensPortraitBackground
    else {
      return false
    }

    var normalizedSettings = oldSnapshot.particleSettings
    normalizedSettings.widensPortraitBackground =
      newSnapshot.particleSettings.widensPortraitBackground
    return normalizedSettings == newSnapshot.particleSettings
  }

  private func isOnlyBackgroundPreviewTimingChange(
    from oldSnapshot: ThemePaletteEditorSnapshot,
    to newSnapshot: ThemePaletteEditorSnapshot
  ) -> Bool {
    guard
      oldSnapshot.sharedPalette == newSnapshot.sharedPalette,
      oldSnapshot.sharedCustomColor == newSnapshot.sharedCustomColor,
      oldSnapshot.sharedMultiColor == newSnapshot.sharedMultiColor,
      oldSnapshot.ribbonPalette == newSnapshot.ribbonPalette,
      oldSnapshot.ribbonCustomColor == newSnapshot.ribbonCustomColor,
      oldSnapshot.ribbonMultiColor == newSnapshot.ribbonMultiColor,
      oldSnapshot.particleSettings.backgroundPreviewBeforeApplyingDuration
        != newSnapshot.particleSettings.backgroundPreviewBeforeApplyingDuration
        || oldSnapshot.particleSettings.backgroundPreviewAfterApplyingDuration
          != newSnapshot.particleSettings.backgroundPreviewAfterApplyingDuration
    else {
      return false
    }

    var normalizedSettings = oldSnapshot.particleSettings
    normalizedSettings.backgroundPreviewBeforeApplyingDuration =
      newSnapshot.particleSettings.backgroundPreviewBeforeApplyingDuration
    normalizedSettings.backgroundPreviewAfterApplyingDuration =
      newSnapshot.particleSettings.backgroundPreviewAfterApplyingDuration
    return normalizedSettings == newSnapshot.particleSettings
  }

  private func tearDownEditor() {
    if !isClosingEditor {
      onSaveAppearance()
      isClosingEditor = true
    }

    previewDebounceTask?.cancel()
    backgroundPreviewTask?.cancel()
    previewFadeTask?.cancel()
    historyCommitTask?.cancel()
    pendingOriginalTheme = nil
    pendingUpdatedTheme = nil
    activeUpdatedTheme = nil
    previewThemeSnapshot = nil
    previewShowsUpdatedTheme = false
    activeSliderTitle = nil
    activeSliderValue = nil
    backgroundPreviewOpacity = 0
    isShowingBackgroundOnly = false
  }

  private func recordEditorChange() {
    let updatedSnapshot = currentEditorSnapshot
    let previousSnapshot = lastEditorSnapshot ?? updatedSnapshot
    guard updatedSnapshot != previousSnapshot else { return }

    if pendingUndoSnapshot == nil {
      pendingUndoSnapshot = previousSnapshot
    }
    lastEditorSnapshot = updatedSnapshot
    redoSnapshots.removeAll()

    historyCommitTask?.cancel()
    historyCommitTask = Task { @MainActor in
      try? await Task.sleep(for: .milliseconds(300))
      guard !Task.isCancelled else { return }
      commitPendingUndoSnapshot()
    }
  }

  private func commitPendingUndoSnapshot() {
    guard let snapshot = pendingUndoSnapshot else { return }
    pendingUndoSnapshot = nil
    undoSnapshots.append(snapshot)
    if undoSnapshots.count > 64 {
      undoSnapshots.removeFirst(undoSnapshots.count - 64)
    }
  }

  private func revertLastChange() {
    historyCommitTask?.cancel()
    commitPendingUndoSnapshot()
    guard let snapshot = undoSnapshots.popLast() else { return }

    redoSnapshots.append(currentEditorSnapshot)
    applyEditorSnapshot(snapshot)
  }

  private func redoLastChange() {
    historyCommitTask?.cancel()
    guard let snapshot = redoSnapshots.popLast() else { return }

    undoSnapshots.append(currentEditorSnapshot)
    applyEditorSnapshot(snapshot)
  }

  private func applyEditorSnapshot(_ snapshot: ThemePaletteEditorSnapshot) {
    pendingUndoSnapshot = nil
    sharedPalette = snapshot.sharedPalette
    sharedCustomColor = snapshot.sharedCustomColor
    sharedMultiColor = snapshot.sharedMultiColor
    ribbonPalette = snapshot.ribbonPalette
    ribbonCustomColor = snapshot.ribbonCustomColor
    ribbonMultiColor = snapshot.ribbonMultiColor
    particleSettings = snapshot.particleSettings
    isPlayStation3XMBPresetExplicit = snapshot.isPlayStation3XMBPresetExplicit
    savedColorsJSON = snapshot.savedColorsJSON
    lastEditorSnapshot = snapshot
  }

  private func resetDynamicBackgroundSettingsAndPalettes() {
    let defaults = DynamicAppearancePreferences.standard
    sharedPalette = defaults.sharedPalette
    sharedCustomColor = defaults.sharedCustomColor
    sharedMultiColor = defaults.sharedMultiColor
    ribbonPalette = defaults.ribbonPalette
    ribbonCustomColor = defaults.ribbonCustomColor
    ribbonMultiColor = defaults.ribbonMultiColor
    particleSettings = defaults.particleSettings
    isPlayStation3XMBPresetExplicit = defaults.isPlayStation3XMBPresetExplicit
  }

  private func cancelChanges() {
    isClosingEditor = true
    endBackgroundPreview()
    if let initialEditorSnapshot {
      applyEditorSnapshot(initialEditorSnapshot)
    }
    dismiss()
  }

  private func applyChanges() {
    isClosingEditor = true
    endBackgroundPreview()
    onSaveAppearance()
    dismiss()
  }
}
