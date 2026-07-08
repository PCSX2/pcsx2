// ControllerButtons.swift — PSBtn, PadBtn, PadButtonFace, DPadView, ActionButtonsView, StickView
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit

// MARK: - D-Pad
struct DPadView: View {
    let size: CGFloat
    @Environment(\.padOpacity) private var padOpacity

    var body: some View {
        let a = size * 0.42
        let sp = size * 0.29
        ZStack {
            PadBtn(label: "▲", w: a, h: a, btn: .up).offset(y: -sp)
            PadBtn(label: "▼", w: a, h: a, btn: .down).offset(y: sp)
            PadBtn(label: "◀", w: a, h: a, btn: .left).offset(x: -sp)
            PadBtn(label: "▶", w: a, h: a, btn: .right).offset(x: sp)
        }
        .environment(\.padOpacity, padOpacity)
    }
}

// MARK: - Action Buttons
struct ActionButtonsView: View {
    let size: CGFloat
    @Environment(\.padOpacity) private var padOpacity

    var body: some View {
        let sp = size * 1.1
        ZStack {
            PSBtn(sym: "△", clr: .green, sz: size, btn: .triangle).offset(y: -sp)
            PSBtn(sym: "✕", clr: .blue, sz: size, btn: .cross).offset(y: sp)
            PSBtn(sym: "□", clr: .pink, sz: size, btn: .square).offset(x: -sp)
            PSBtn(sym: "○", clr: .red, sz: size, btn: .circle).offset(x: sp)
        }
        .environment(\.padOpacity, padOpacity)
    }
}

struct PSBtn: View {
    let sym: String; let clr: Color; let sz: CGFloat; let btn: ARMSX2PadButton
    var visibleScaleX: CGFloat = 1.0
    var visibleScaleY: CGFloat = 1.0
    var hitScaleX: CGFloat = 1.0
    var hitScaleY: CGFloat = 1.0
    @State private var on = false
    @Environment(\.padOpacity) private var padOpacity
    @Environment(\.padSkin) private var padSkin
    @Environment(\.padSkinDescriptor) private var padSkinDescriptor
    @Environment(\.padUsesFullSkin) private var padUsesFullSkin

    private var accessibilityName: String {
        switch btn {
        case .triangle: return "Triangle"
        case .cross: return "Cross"
        case .square: return "Square"
        case .circle: return "Circle"
        default: return sym
        }
    }

    private var visibleW: CGFloat {
        PadLayoutMetrics.visibleLength(baseLength: sz, visibleScale: visibleScaleX)
    }

    private var visibleH: CGFloat {
        PadLayoutMetrics.visibleLength(baseLength: sz, visibleScale: visibleScaleY)
    }

    private var touchW: CGFloat {
        PadLayoutMetrics.touchLength(baseLength: sz, hitScale: hitScaleX)
    }

    private var touchH: CGFloat {
        PadLayoutMetrics.touchLength(baseLength: sz, hitScale: hitScaleY)
    }

    var body: some View {
        Group {
            if ARMSX2UsesUIKitPadPressSurface() {
                ZStack {
                    UIKitPadPressSurface(onPress: updatePressed) {
                        centeredButtonFace
                    }
                    .frame(width: touchW, height: touchH)
                }
                .frame(width: touchW, height: touchH)
                .opacity(padUsesFullSkin ? 1.0 : padOpacity)
                .animation(.easeOut(duration: 0.06), value: on)
            } else {
                centeredButtonFace
                .frame(width: touchW, height: touchH)
                .contentShape(Rectangle())
                .opacity(padUsesFullSkin ? 1.0 : padOpacity)
                .animation(.easeOut(duration: 0.06), value: on)
                .simultaneousGesture(DragGesture(minimumDistance: 0)
                    .onChanged { _ in updatePressed(true) }
                    .onEnded { _ in updatePressed(false) })
            }
        }
        .onDisappear {
            updatePressed(false)
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(accessibilityName)
        .accessibilityAddTraits(.isButton)
    }

    private var centeredButtonFace: some View {
        ZStack(alignment: .topLeading) {
            Color.clear
                .frame(width: touchW, height: touchH)

            buttonFace
                .frame(width: visibleW, height: visibleH)
                .position(x: touchW / 2, y: touchH / 2)
        }
        .frame(width: touchW, height: touchH)
    }

    private var buttonFace: some View {
        ZStack {
            // Ellipse (not Circle) so the press-mask shape matches a non-uniform
            // visible frame; degenerates to a circle when the axes are equal.
            Ellipse()
                .fill(.clear)

            if !padUsesFullSkin || on {
                ARMSX2SkinMaskPressEffect(button: btn, skin: padSkin, descriptor: padSkinDescriptor, color: clr, isPressed: on, opacity: padUsesFullSkin ? padOpacity * 0.75 : padOpacity)
            }

            if !padUsesFullSkin {
                ControllerAssetImage(
                    fileName: ControllerAsset.fileName(for: btn),
                    fallback: sym,
                    fallbackColor: on ? .white : clr,
                    fallbackFontSize: min(visibleW, visibleH) * 0.42,
                    skin: padSkin,
                    descriptor: padSkinDescriptor
                )
                    .padding(padSkin == .crispVector ? 0 : max(1, min(visibleW, visibleH) * 0.03))
                    .brightness(on ? 0.18 : 0)
                    .saturation(on ? 1.16 : 1.0)
                    .scaleEffect(on ? 0.90 : 1.0)
            }
        }
    }

    private func updatePressed(_ pressed: Bool) {
        guard on != pressed else {
            return
        }

        on = pressed
        EmulatorBridge.shared.setPadButton(btn, pressed: pressed)
        if pressed && SettingsStore.shared.hapticFeedback {
            HapticManager.medium.impactOccurred()
        }
    }
}

// Reusable render-only visual for a pad button. PadBtn drives its own press
// state; the composite D-pad drives this view directly for diagonal highlighting.
struct PadButtonFace: View {
    let button: ARMSX2PadButton
    let label: String
    let baseW: CGFloat
    let baseH: CGFloat
    var visibleScaleX: CGFloat = 1.0
    var visibleScaleY: CGFloat = 1.0
    var hitScaleX: CGFloat = 1.0
    var hitScaleY: CGFloat = 1.0
    let isPressed: Bool
    @Environment(\.padOpacity) private var padOpacity
    @Environment(\.padSkin) private var padSkin
    @Environment(\.padSkinDescriptor) private var padSkinDescriptor
    @Environment(\.padUsesFullSkin) private var padUsesFullSkin

    private var visibleW: CGFloat {
        PadLayoutMetrics.visibleLength(baseLength: baseW, visibleScale: visibleScaleX)
    }

    private var visibleH: CGFloat {
        PadLayoutMetrics.visibleLength(baseLength: baseH, visibleScale: visibleScaleY)
    }

    private var touchW: CGFloat {
        PadLayoutMetrics.touchLength(baseLength: baseW, hitScale: hitScaleX)
    }

    private var touchH: CGFloat {
        PadLayoutMetrics.touchLength(baseLength: baseH, hitScale: hitScaleY)
    }

    var body: some View {
        centeredButtonFace
    }

    private var centeredButtonFace: some View {
        ZStack(alignment: .topLeading) {
            Color.clear
                .frame(width: touchW, height: touchH)

            buttonFace
                .frame(width: visibleW, height: visibleH)
                .position(x: touchW / 2, y: touchH / 2)
        }
        .frame(width: touchW, height: touchH)
    }

    private var buttonFace: some View {
        let shape = RoundedRectangle(cornerRadius: min(visibleW, visibleH) * 0.28, style: .continuous)
        return ZStack {
            shape
                .fill(.clear)

            if !padUsesFullSkin || isPressed {
                ARMSX2SkinMaskPressEffect(button: button, skin: padSkin, descriptor: padSkinDescriptor, color: .white, isPressed: isPressed, opacity: padUsesFullSkin ? padOpacity * 0.75 : padOpacity)
            }

            if !padUsesFullSkin {
                ControllerAssetImage(
                    fileName: ControllerAsset.fileName(for: button),
                    fallback: label,
                    fallbackColor: isPressed ? .black : .white,
                    fallbackFontSize: min(visibleW, visibleH) * 0.38,
                    skin: padSkin,
                    descriptor: padSkinDescriptor
                )
                    .padding(padSkin == .crispVector ? 0 : max(1, min(visibleW, visibleH) * 0.03))
                    .brightness(isPressed ? 0.18 : 0)
                    .scaleEffect(isPressed ? 0.91 : 1.0)
            }
        }
    }
}

struct PadBtn: View {
    let label: String; let w: CGFloat; let h: CGFloat; let btn: ARMSX2PadButton
    var visibleScaleX: CGFloat = 1.0
    var visibleScaleY: CGFloat = 1.0
    var hitScaleX: CGFloat = 1.0
    var hitScaleY: CGFloat = 1.0
    @State private var on = false
    @Environment(\.padOpacity) private var padOpacity
    @Environment(\.padUsesFullSkin) private var padUsesFullSkin

    private var accessibilityName: String {
        switch btn {
        case .up: return "D-pad up"
        case .down: return "D-pad down"
        case .left: return "D-pad left"
        case .right: return "D-pad right"
        case .triangle: return "Triangle"
        case .cross: return "Cross"
        case .square: return "Square"
        case .circle: return "Circle"
        case .L1: return "L1"
        case .R1: return "R1"
        case .L2: return "L2"
        case .R2: return "R2"
        case .start: return "Start"
        case .select: return "Select"
        case .L3: return "L3"
        case .R3: return "R3"
        @unknown default: return label
        }
    }

    private var touchW: CGFloat {
        PadLayoutMetrics.touchLength(baseLength: w, hitScale: hitScaleX)
    }

    private var touchH: CGFloat {
        PadLayoutMetrics.touchLength(baseLength: h, hitScale: hitScaleY)
    }

    private var face: some View {
        PadButtonFace(button: btn, label: label, baseW: w, baseH: h, visibleScaleX: visibleScaleX, visibleScaleY: visibleScaleY, hitScaleX: hitScaleX, hitScaleY: hitScaleY, isPressed: on)
    }

    var body: some View {
        Group {
            if ARMSX2UsesUIKitPadPressSurface() {
                ZStack {
                    UIKitPadPressSurface(onPress: updatePressed) {
                        face
                    }
                    .frame(width: touchW, height: touchH)
                }
                .frame(width: touchW, height: touchH)
                .opacity(padUsesFullSkin ? 1.0 : padOpacity)
                .animation(.easeOut(duration: 0.06), value: on)
            } else {
                face
                .frame(width: touchW, height: touchH)
                .contentShape(Rectangle())
                .opacity(padUsesFullSkin ? 1.0 : padOpacity)
                .animation(.easeOut(duration: 0.06), value: on)
                .simultaneousGesture(DragGesture(minimumDistance: 0)
                    .onChanged { _ in updatePressed(true) }
                    .onEnded { _ in updatePressed(false) })
            }
        }
        .onDisappear {
            updatePressed(false)
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(accessibilityName)
        .accessibilityAddTraits(.isButton)
    }

    private func updatePressed(_ pressed: Bool) {
        guard on != pressed else {
            return
        }

        on = pressed
        EmulatorBridge.shared.setPadButton(btn, pressed: pressed)
        if pressed && SettingsStore.shared.hapticFeedback {
            HapticManager.medium.impactOccurred()
        }
    }
}

// MARK: - Analog Stick with L3/R3 tap
struct StickView: View {
    let isLeft: Bool
    let sizeScale: CGFloat
    var layoutScale: CGFloat = 1.0
    var captureDiameter: CGFloat? = nil
    var knobImage: UIImage? = nil
    var knobSize: CGSize? = nil

    private var clampedScale: CGFloat {
        min(max(sizeScale, 0.8), 1.6)
    }
    private var effectiveScale: CGFloat {
        clampedScale * PadLayoutMetrics.clampedScale(layoutScale)
    }
    private var sz: CGFloat {
        68 * effectiveScale
    }
    private var knob: CGFloat {
        30 * effectiveScale
    }
    private var effectiveCapture: CGFloat {
        max(captureDiameter ?? sz, sz)
    }

    @State private var off: CGSize = .zero
    @State private var isDragging = false
    @Environment(\.padOpacity) private var padOpacity
    @Environment(\.padSkin) private var padSkin
    @Environment(\.padSkinDescriptor) private var padSkinDescriptor
    @Environment(\.padUsesFullSkin) private var padUsesFullSkin

    var body: some View {
        ZStack {
            Circle()
                .fill(.clear)
                .frame(width: sz, height: sz)

            if isDragging {
                Circle()
                    .fill(.black.opacity((isDragging ? 0.26 : 0.18) * padOpacity))
                    .stroke(.white.opacity((isDragging ? 0.34 : 0.18) * padOpacity), lineWidth: isDragging ? 1.8 : 1)
                    .shadow(color: .white.opacity(isDragging ? 0.22 * padOpacity : 0.05 * padOpacity), radius: isDragging ? 8 : 2)
                    .frame(width: sz, height: sz)
            }

            if !padUsesFullSkin {
                ControllerAssetImage(
                    fileName: ControllerAsset.analogBaseFileName(isLeft: isLeft, descriptor: padSkinDescriptor),
                    fallback: "",
                    fallbackColor: .white,
                    fallbackFontSize: 1,
                    skin: padSkin,
                    descriptor: padSkinDescriptor
                )
                    .frame(width: sz, height: sz)
                    .opacity(padOpacity)
                ControllerAssetImage(
                    fileName: ControllerAsset.analogStickFileName(isLeft: isLeft, descriptor: padSkinDescriptor),
                    fallback: "",
                    fallbackColor: .white,
                    fallbackFontSize: 1,
                    skin: padSkin,
                    descriptor: padSkinDescriptor
                )
                    .frame(width: knob, height: knob)
                    .opacity(padOpacity)
                    .brightness(isDragging ? 0.18 : 0)
                    .scaleEffect(isDragging ? 1.08 : 1.0)
                    .offset(off)
                ControllerAssetImage(
                    fileName: isLeft ? "ic_controller_l3_button.png" : "ic_controller_r3_button.png",
                    fallback: isLeft ? "L3" : "R3",
                    fallbackColor: .white.opacity(0.35),
                    fallbackFontSize: 9,
                    skin: padSkin,
                    descriptor: padSkinDescriptor
                )
                    .frame(width: 18, height: 18)
                    .opacity(0.45 * padOpacity)
                    .offset(y: sz / 2 + 9)
            } else if let knobImage {
                Image(uiImage: knobImage)
                    .resizable()
                    .interpolation(.high)
                    .antialiased(true)
                    .scaledToFit()
                    .frame(width: knobSize?.width ?? knob, height: knobSize?.height ?? knob)
                    .opacity(padOpacity)
                    .brightness(isDragging ? 0.12 : 0)
                    .scaleEffect(isDragging ? 1.06 : 1.0)
                    .offset(off)
            } else if isDragging {
                Circle()
                    .fill(.white.opacity(0.22 * padOpacity))
                    .stroke(.white.opacity(0.34 * padOpacity), lineWidth: 1.4)
                    .frame(width: knob, height: knob)
                    .offset(off)
            }
        }
        .frame(width: effectiveCapture, height: effectiveCapture)
        .contentShape(Circle())
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(isLeft ? "Left stick" : "Right stick")
        .accessibilityAddTraits(.isButton)
        .simultaneousGesture(DragGesture(minimumDistance: 0)
            .onChanged { v in
                let maxR = (sz - knob) / 2
                let dist = hypot(v.translation.width, v.translation.height)
                if dist > 4 {
                    isDragging = true
                    let d = min(dist, maxR)
                    let a = atan2(v.translation.height, v.translation.width)
                    off = CGSize(width: cos(a) * d, height: sin(a) * d)
                    let nx = Float(cos(a) * d / maxR); let ny = Float(sin(a) * d / maxR)
                    isLeft ? EmulatorBridge.shared.setLeftStick(x: nx, y: ny)
                           : EmulatorBridge.shared.setRightStick(x: nx, y: ny)
                }
            }
            .onEnded { _ in
                if !isDragging {
                    // Tap (no significant drag) → L3/R3 press
                    let btn: ARMSX2PadButton = isLeft ? .L3 : .R3
                    EmulatorBridge.shared.setPadButton(btn, pressed: true)
                    if SettingsStore.shared.hapticFeedback {
                        HapticManager.light.impactOccurred()
                    }
                    DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
                        EmulatorBridge.shared.setPadButton(btn, pressed: false)
                    }
                } else {
                    // Drag ended → reset stick
                    withAnimation(.spring(duration: 0.12)) { off = .zero }
                    isLeft ? EmulatorBridge.shared.setLeftStick(x: 0, y: 0)
                           : EmulatorBridge.shared.setRightStick(x: 0, y: 0)
                }
                isDragging = false
            })
    }
}
