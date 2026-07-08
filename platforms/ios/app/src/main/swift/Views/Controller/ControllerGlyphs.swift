// ControllerGlyphs.swift — controller asset image view, vector glyphs, and shapes
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct ControllerAssetImage: View {
    let fileName: String
    let fallback: String
    let fallbackColor: Color
    let fallbackFontSize: CGFloat
    let skin: VirtualPadSkin
    var descriptor: VPadSkinDescriptor? = nil

    var body: some View {
        if skin == .crispVector {
            ControllerVectorGlyph(
                fileName: fileName,
                fallback: fallback,
                fallbackColor: fallbackColor,
                fallbackFontSize: fallbackFontSize
            )
        } else if let descriptor,
                  let image = ControllerAsset.image(named: fileName, descriptor: descriptor) {
            Image(uiImage: image)
                .resizable()
                .interpolation(.high)
                .antialiased(true)
                .scaledToFit()
        } else if descriptor == nil,
                  let image = ControllerAsset.image(named: fileName, skin: skin) {
            Image(uiImage: image)
                .resizable()
                .interpolation(.high)
                .antialiased(true)
                .scaledToFit()
        } else {
            Text(fallback)
                .font(.system(size: fallbackFontSize, weight: .semibold))
                .foregroundStyle(fallbackColor)
                .minimumScaleFactor(0.5)
        }
    }
}

private struct ControllerVectorGlyph: View {
    let fileName: String
    let fallback: String
    let fallbackColor: Color
    let fallbackFontSize: CGFloat

    var body: some View {
        let lowerName = fileName.lowercased()

        GeometryReader { geo in
            if lowerName.contains("analog_base") {
                AnalogBaseGlyph()
            } else if lowerName.contains("analog_stick") || lowerName.contains("analog_button") {
                AnalogStickGlyph()
            } else if let face = FaceGlyph.Kind(fileName: lowerName) {
                FaceGlyph(kind: face)
            } else if let direction = DPadGlyph.Direction(fileName: lowerName) {
                DPadGlyph(direction: direction)
            } else if lowerName.contains("start") {
                CapsuleGlyph(label: fallback.isEmpty ? "START" : fallback, symbol: .play)
            } else if lowerName.contains("select") {
                CapsuleGlyph(label: fallback.isEmpty ? "SEL" : fallback, symbol: .minus)
            } else if lowerName.contains("l3") || lowerName.contains("r3") {
                CircleLabelGlyph(label: fallback)
            } else if lowerName.contains("l1") || lowerName.contains("l2") || lowerName.contains("r1") || lowerName.contains("r2") {
                ShoulderGlyph(label: fallback)
            } else {
                Text(fallback)
                    .font(.system(size: fallbackFontSize, weight: .semibold))
                    .foregroundStyle(fallbackColor)
                    .minimumScaleFactor(0.5)
            }
        }
    }
}

private struct FaceGlyph: View {
    enum Kind {
        case cross
        case circle
        case square
        case triangle

        init?(fileName: String) {
            if fileName.contains("cross") {
                self = .cross
            } else if fileName.contains("circle") {
                self = .circle
            } else if fileName.contains("square") {
                self = .square
            } else if fileName.contains("triangle") {
                self = .triangle
            } else {
                return nil
            }
        }

        var color: Color {
            switch self {
            case .cross:
                return Color(red: 0.18, green: 0.43, blue: 1.0)
            case .circle:
                return Color(red: 0.86, green: 0.0, blue: 0.0)
            case .square:
                return Color(red: 1.0, green: 0.0, blue: 1.0)
            case .triangle:
                return Color(red: 0.0, green: 0.78, blue: 0.33)
            }
        }
    }

    let kind: Kind

    var body: some View {
        GeometryReader { geo in
            let side = min(geo.size.width, geo.size.height)
            let lineWidth = max(2, side * 0.08)

            ZStack {
                Circle()
                    .fill(.black.opacity(0.08))
                    .stroke(.white.opacity(0.52), lineWidth: max(1.2, side * 0.04))

                switch kind {
                case .cross:
                    CrossShape()
                        .stroke(kind.color, style: StrokeStyle(lineWidth: lineWidth, lineCap: .round))
                        .frame(width: side * 0.42, height: side * 0.42)
                case .circle:
                    Circle()
                        .stroke(kind.color, lineWidth: lineWidth)
                        .frame(width: side * 0.48, height: side * 0.48)
                case .square:
                    Rectangle()
                        .stroke(kind.color, lineWidth: lineWidth)
                        .frame(width: side * 0.46, height: side * 0.46)
                case .triangle:
                    TriangleShape()
                        .stroke(kind.color, style: StrokeStyle(lineWidth: lineWidth, lineJoin: .round))
                        .frame(width: side * 0.58, height: side * 0.48)
                        .offset(y: -side * 0.02)
                }
            }
            .frame(width: geo.size.width, height: geo.size.height)
        }
    }
}

private struct DPadGlyph: View {
    enum Direction {
        case up
        case down
        case left
        case right

        init?(fileName: String) {
            if fileName.contains("_up_") {
                self = .up
            } else if fileName.contains("_down_") {
                self = .down
            } else if fileName.contains("_left_") {
                self = .left
            } else if fileName.contains("_right_") {
                self = .right
            } else {
                return nil
            }
        }

        var angle: Angle {
            switch self {
            case .up:
                return .degrees(0)
            case .right:
                return .degrees(90)
            case .down:
                return .degrees(180)
            case .left:
                return .degrees(270)
            }
        }
    }

    let direction: Direction

    var body: some View {
        GeometryReader { geo in
            let side = min(geo.size.width, geo.size.height)

            ZStack {
                RoundedRectangle(cornerRadius: side * 0.20, style: .continuous)
                    .fill(.black.opacity(0.18))
                    .stroke(.white.opacity(0.22), lineWidth: max(1.2, side * 0.045))

                TriangleShape()
                    .fill(.white.opacity(0.72))
                    .frame(width: side * 0.30, height: side * 0.24)
                    .rotationEffect(direction.angle)
            }
            .frame(width: geo.size.width, height: geo.size.height)
        }
    }
}

private struct ShoulderGlyph: View {
    let label: String

    var body: some View {
        GeometryReader { geo in
            let corner = min(geo.size.height * 0.28, 14)

            ZStack {
                RoundedRectangle(cornerRadius: corner, style: .continuous)
                    .fill(.black.opacity(0.16))
                    .stroke(.white.opacity(0.28), lineWidth: 1.6)

                Text(label)
                    .font(.system(size: max(11, geo.size.height * 0.42), weight: .semibold, design: .rounded))
                    .foregroundStyle(.white.opacity(0.76))
                    .minimumScaleFactor(0.55)
            }
        }
    }
}

private struct CapsuleGlyph: View {
    enum Symbol {
        case play
        case minus
    }

    let label: String
    let symbol: Symbol

    var body: some View {
        GeometryReader { geo in
            ZStack {
                Capsule()
                    .fill(.black.opacity(0.14))
                    .stroke(.white.opacity(0.26), lineWidth: 1.4)

                if geo.size.width < 34 {
                    symbolView
                } else {
                    Text(label)
                        .font(.system(size: max(8, geo.size.height * 0.42), weight: .semibold, design: .rounded))
                        .foregroundStyle(.white.opacity(0.72))
                        .minimumScaleFactor(0.45)
                }
            }
        }
    }

    @ViewBuilder
    private var symbolView: some View {
        switch symbol {
        case .play:
            TriangleShape()
                .fill(.white.opacity(0.72))
                .rotationEffect(.degrees(90))
                .padding(6)
        case .minus:
            Capsule()
                .fill(.white.opacity(0.72))
                .frame(width: 14, height: 3)
        }
    }
}

private struct CircleLabelGlyph: View {
    let label: String

    var body: some View {
        GeometryReader { geo in
            let side = min(geo.size.width, geo.size.height)

            ZStack {
                Circle()
                    .fill(.black.opacity(0.16))
                    .stroke(.white.opacity(0.26), lineWidth: max(1, side * 0.055))

                Text(label)
                    .font(.system(size: max(6, side * 0.32), weight: .bold, design: .rounded))
                    .foregroundStyle(.white.opacity(0.54))
                    .minimumScaleFactor(0.4)
            }
            .frame(width: geo.size.width, height: geo.size.height)
        }
    }
}

private struct AnalogBaseGlyph: View {
    var body: some View {
        GeometryReader { geo in
            let side = min(geo.size.width, geo.size.height)

            ZStack {
                Circle()
                    .fill(.black.opacity(0.14))
                    .stroke(.white.opacity(0.20), lineWidth: max(1, side * 0.018))

                Circle()
                    .stroke(.white.opacity(0.08), lineWidth: max(1, side * 0.035))
                    .frame(width: side * 0.78, height: side * 0.78)
            }
            .frame(width: geo.size.width, height: geo.size.height)
        }
    }
}

private struct AnalogStickGlyph: View {
    var body: some View {
        GeometryReader { geo in
            ZStack {
                Circle()
                    .fill(.white.opacity(0.22))
                    .stroke(.white.opacity(0.18), lineWidth: 1.2)

                Circle()
                    .fill(.white.opacity(0.10))
                    .padding(6)
            }
            .frame(width: geo.size.width, height: geo.size.height)
        }
    }
}

private struct CrossShape: Shape {
    func path(in rect: CGRect) -> Path {
        var path = Path()
        path.move(to: CGPoint(x: rect.minX, y: rect.minY))
        path.addLine(to: CGPoint(x: rect.maxX, y: rect.maxY))
        path.move(to: CGPoint(x: rect.maxX, y: rect.minY))
        path.addLine(to: CGPoint(x: rect.minX, y: rect.maxY))
        return path
    }
}

private struct TriangleShape: Shape {
    func path(in rect: CGRect) -> Path {
        var path = Path()
        path.move(to: CGPoint(x: rect.midX, y: rect.minY))
        path.addLine(to: CGPoint(x: rect.maxX, y: rect.maxY))
        path.addLine(to: CGPoint(x: rect.minX, y: rect.maxY))
        path.closeSubpath()
        return path
    }
}
