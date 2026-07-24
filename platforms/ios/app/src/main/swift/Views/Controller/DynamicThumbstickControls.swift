// DynamicThumbstickControls.swift — Floating sticks, swipe camera, gyro, and touch actions
// SPDX-License-Identifier: GPL-3.0+

import CoreMotion
import QuartzCore
import SwiftUI
import UIKit

struct DynamicThumbstickVector: Equatable, Sendable {
    var x: CGFloat
    var y: CGFloat

    static let zero = DynamicThumbstickVector(x: 0, y: 0)

    var magnitude: CGFloat {
        hypot(x, y)
    }

    func limited(to maximumMagnitude: CGFloat) -> DynamicThumbstickVector {
        let magnitude = magnitude
        guard magnitude > maximumMagnitude, magnitude > 0 else { return self }
        let scale = maximumMagnitude / magnitude
        return DynamicThumbstickVector(x: x * scale, y: y * scale)
    }
}

enum DynamicCrosshairMotionSource: Hashable {
    case swipe
    case thumbstick
    case gyroscope
}

struct DynamicThumbstickSample {
    let input: DynamicThumbstickVector
    let rawDistance: CGFloat
}

enum DynamicThumbstickMath {
    static func sample(translation: CGSize, maximumRadius: CGFloat, deadZone: CGFloat) -> DynamicThumbstickSample {
        guard maximumRadius > 0 else {
            return DynamicThumbstickSample(input: .zero, rawDistance: 0)
        }

        let rawDistance = hypot(translation.width, translation.height)
        guard rawDistance > 0 else {
            return DynamicThumbstickSample(input: .zero, rawDistance: 0)
        }

        let safeDeadZone = min(max(deadZone, 0), 0.95)
        let cappedDistance = min(rawDistance, maximumRadius)
        let normalizedDistance = cappedDistance / maximumRadius
        // Begin at a true 0% deadzone so even a tiny drag produces input. The
        // configured deadzone grows with travel and reaches its selected value
        // at the outer radius, while the remap still preserves full-scale output.
        let adaptiveDeadZone = safeDeadZone * normalizedDistance
        let magnitude = (normalizedDistance - adaptiveDeadZone) / (1 - adaptiveDeadZone)

        return DynamicThumbstickSample(
            input: DynamicThumbstickVector(
                x: translation.width / rawDistance * magnitude,
                // ARMSX2's existing virtual sticks use screen-space positive Y.
                y: translation.height / rawDistance * magnitude
            ),
            rawDistance: rawDistance
        )
    }

    static func trailDotScale(index: Int, rawDistance: CGFloat, maximumRadius: CGFloat, maximumDots: Int = 7) -> CGFloat {
        guard index >= 0, index < maximumDots, rawDistance > 0, maximumRadius > 0 else { return 0 }
        let progress = min(rawDistance / maximumRadius, 1) * CGFloat(maximumDots)
        return min(max(progress - CGFloat(index), 0), 1)
    }

    static func trailDotPositionProgress(index: Int, rawDistance: CGFloat, maximumRadius: CGFloat, maximumDots: Int = 7) -> CGFloat {
        let scale = trailDotScale(index: index, rawDistance: rawDistance, maximumRadius: maximumRadius, maximumDots: maximumDots)
        guard scale > 0 else { return 0 }
        return CGFloat(index + 1) / CGFloat(maximumDots + 1) * scale
    }
}

struct DynamicThumbstickView: View {
    let isLeft: Bool
    let radius: CGFloat
    let deadZone: CGFloat
    let hapticsEnabled: Bool
    let thumbstickOpacity: Double
    let baseOpacity: Double
    let trailOpacity: Double
    var tapActionsEnabled = false
    var maximumTapDuration: TimeInterval = 0.30
    var tapTravelTolerance: CGFloat = 12
    var onVector: (DynamicThumbstickVector) -> Void
    var onInteractionBegan: () -> Void = {}
    var onInteractionActivity: () -> Void = {}
    var onInteractionTap: () -> Void = {}
    var onInteractionEnded: () -> Void = {}

    @State private var origin = CGPoint.zero
    @State private var dragOffset = CGSize.zero
    @State private var dragDistance: CGFloat = 0
    @State private var isActive = false
    @State private var gestureStartedAt: Date?
    @State private var maximumTravel: CGFloat = 0

    var body: some View {
        GeometryReader { _ in
            ZStack(alignment: .topLeading) {
                Color.clear
                    .contentShape(Rectangle())

                if isActive {
                    DynamicThumbstickVisual(
                        radius: radius,
                        dragOffset: dragOffset,
                        dragDistance: dragDistance,
                        thumbstickOpacity: thumbstickOpacity,
                        baseOpacity: baseOpacity,
                        trailOpacity: trailOpacity
                    )
                    .position(origin)
                    .transition(.opacity)
                    .allowsHitTesting(false)
                }
            }
            .gesture(dragGesture)
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(isLeft ? "Dynamic left thumbstick area" : "Dynamic right thumbstick area")
        .accessibilityHint("Press and drag anywhere in this area")
        .onDisappear(perform: reset)
    }

    private var dragGesture: some Gesture {
        DragGesture(minimumDistance: 0, coordinateSpace: .local)
            .onChanged { value in
                if !isActive {
                    origin = value.startLocation
                    dragOffset = .zero
                    dragDistance = 0
                    maximumTravel = 0
                    gestureStartedAt = value.time
                    if tapActionsEnabled { onInteractionBegan() }
                    withAnimation(.spring(response: 0.20, dampingFraction: 0.78)) {
                        isActive = true
                    }
                    if hapticsEnabled && SettingsStore.shared.hapticFeedback {
                        HapticManager.light.impactOccurred(intensity: 0.72)
                    }
                }

                maximumTravel = max(maximumTravel, hypot(value.translation.width, value.translation.height))
                if tapActionsEnabled { onInteractionActivity() }
                let sample = DynamicThumbstickMath.sample(
                    translation: value.translation,
                    maximumRadius: radius,
                    deadZone: deadZone
                )
                dragOffset = value.translation
                dragDistance = sample.rawDistance
                onVector(sample.input)
            }
            .onEnded { value in
                let duration = value.time.timeIntervalSince(gestureStartedAt ?? value.time)
                let travel = max(maximumTravel, hypot(value.translation.width, value.translation.height))
                if duration <= maximumTapDuration && travel <= tapTravelTolerance {
                    if tapActionsEnabled {
                        onInteractionTap()
                    } else {
                        pulseStickButton()
                    }
                }
                reset()
            }
    }

    private func pulseStickButton() {
        let button: ARMSX2PadButton = isLeft ? .L3 : .R3
        EmulatorBridge.shared.setPadButton(button, pressed: true)
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.10) {
            EmulatorBridge.shared.setPadButton(button, pressed: false)
        }
    }

    private func reset() {
        guard isActive || gestureStartedAt != nil else {
            onVector(.zero)
            return
        }
        onVector(.zero)
        if tapActionsEnabled && gestureStartedAt != nil { onInteractionEnded() }
        gestureStartedAt = nil
        maximumTravel = 0
        withAnimation(.easeOut(duration: 0.14)) {
            isActive = false
        }
    }
}

private struct DynamicThumbstickVisual: View {
    let radius: CGFloat
    let dragOffset: CGSize
    let dragDistance: CGFloat
    let thumbstickOpacity: Double
    let baseOpacity: Double
    let trailOpacity: Double

    var body: some View {
        ZStack {
            ForEach(0..<7, id: \.self) { index in
                let scale = DynamicThumbstickMath.trailDotScale(index: index, rawDistance: dragDistance, maximumRadius: radius)
                let progress = DynamicThumbstickMath.trailDotPositionProgress(index: index, rawDistance: dragDistance, maximumRadius: radius)
                if scale > 0 {
                    Circle()
                        .fill(Color(white: 0.82).opacity(trailOpacity * Double(scale)))
                        .frame(width: 7.5, height: 7.5)
                        .scaleEffect(scale)
                        .offset(x: dragOffset.width * progress, y: dragOffset.height * progress)
                        .animation(.spring(response: 0.18, dampingFraction: 0.72), value: scale)
                }
            }

            Circle()
                .fill(Color(white: 0.72).opacity(baseOpacity))
                .frame(width: radius * 0.42, height: radius * 0.42)

            Circle()
                .fill(Color(white: 0.88).opacity(thumbstickOpacity))
                .frame(width: radius * 0.64, height: radius * 0.64)
                .offset(dragOffset)
        }
        .frame(width: radius * 2, height: radius * 2)
    }
}

struct VirtualPadCameraSwipeView: View {
    let maximumTapDuration: TimeInterval
    let tapTravelTolerance: CGFloat
    let onDelta: (CGSize) -> Void
    let onBegan: () -> Void
    let onActivity: () -> Void
    let onTap: () -> Void
    let onEnded: () -> Void

    @State private var lastLocation: CGPoint?
    @State private var gestureStartedAt: Date?
    @State private var maximumTravel: CGFloat = 0

    var body: some View {
        GeometryReader { _ in
            Color.clear
                .contentShape(Rectangle())
                .gesture(swipeGesture)
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("Camera swipe area")
        .accessibilityHint("Swipe to move the emulated right analog stick")
        .onDisappear(perform: endInteractionIfNeeded)
    }

    private var swipeGesture: some Gesture {
        DragGesture(minimumDistance: 0, coordinateSpace: .local)
            .onChanged { value in
                if gestureStartedAt == nil {
                    gestureStartedAt = value.time
                    maximumTravel = 0
                    onBegan()
                }
                maximumTravel = max(maximumTravel, hypot(value.translation.width, value.translation.height))
                onActivity()
                defer { lastLocation = value.location }
                guard let lastLocation else { return }
                let delta = CGSize(
                    width: value.location.x - lastLocation.x,
                    height: value.location.y - lastLocation.y
                )
                if abs(delta.width) > 0.01 || abs(delta.height) > 0.01 {
                    onDelta(delta)
                }
            }
            .onEnded { value in
                let duration = value.time.timeIntervalSince(gestureStartedAt ?? value.time)
                let travel = max(maximumTravel, hypot(value.translation.width, value.translation.height))
                if duration <= maximumTapDuration && travel <= tapTravelTolerance { onTap() }
                onEnded()
                reset()
            }
    }

    private func endInteractionIfNeeded() {
        if gestureStartedAt != nil { onEnded() }
        reset()
    }

    private func reset() {
        lastLocation = nil
        gestureStartedAt = nil
        maximumTravel = 0
    }
}

@MainActor
final class SwipeCameraInputDriver: NSObject {
    private var displayLink: CADisplayLink?
    private var pendingDelta = CGSize.zero
    private var pendingDeltaIsAiming = false
    private var lastTimestamp: CFTimeInterval?
    private var outputWasActive = false
    var onCameraMotion: ((DynamicThumbstickVector) -> Void)?

    func start() {
        guard displayLink == nil else { return }
        let link = CADisplayLink(target: self, selector: #selector(update(_:)))
        link.preferredFrameRateRange = CAFrameRateRange(minimum: 30, maximum: 120, preferred: 60)
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    func add(delta: CGSize, isAiming: Bool) {
        pendingDelta.width += delta.width
        pendingDelta.height += delta.height
        pendingDeltaIsAiming = isAiming
    }

    func stop() {
        displayLink?.invalidate()
        displayLink = nil
        pendingDelta = .zero
        pendingDeltaIsAiming = false
        lastTimestamp = nil
        outputWasActive = false
        onCameraMotion?(.zero)
        EmulatorBridge.shared.setRightStick(x: 0, y: 0)
    }

    @objc private func update(_ link: CADisplayLink) {
        let previous = lastTimestamp ?? (link.timestamp - 1.0 / 60.0)
        let deltaTime = min(max(link.timestamp - previous, 1.0 / 240.0), 1.0 / 20.0)
        lastTimestamp = link.timestamp

        let delta = pendingDelta
        let isAiming = pendingDeltaIsAiming
        pendingDelta = .zero
        guard delta != .zero else {
            if outputWasActive {
                outputWasActive = false
                onCameraMotion?(.zero)
                EmulatorBridge.shared.setRightStick(x: 0, y: 0)
            }
            return
        }

        let settings = DynamicThumbstickSettings.shared
        let sensitivity = settings.effectiveSwipeSensitivity(isAiming: isAiming)
        let degreesPerSecondX = Double(delta.width) * sensitivity.horizontal / deltaTime
        let degreesPerSecondY = Double(delta.height) * sensitivity.vertical / deltaTime
        let referenceDegreesPerSecond = 105.0
        let motion = DynamicThumbstickVector(
            x: CGFloat(degreesPerSecondX / referenceDegreesPerSecond),
            y: CGFloat(degreesPerSecondY / referenceDegreesPerSecond)
        ).limited(to: 1)
        outputWasActive = true
        onCameraMotion?(motion)
        EmulatorBridge.shared.setRightStick(
            x: Float(motion.x),
            y: Float(motion.y)
        )
    }
}

@MainActor
final class VirtualPadGyroscopeController {
    private let motionManager = CMMotionManager()
    private var filteredRate = DynamicThumbstickVector.zero
    var onCameraMotion: ((DynamicThumbstickVector) -> Void)?

    var isAvailable: Bool { motionManager.isGyroAvailable }

    init() {
        motionManager.gyroUpdateInterval = 1.0 / 60.0
    }

    func setEnabled(_ enabled: Bool) {
        guard enabled, motionManager.isGyroAvailable else {
            stop()
            return
        }
        guard !motionManager.isGyroActive else { return }

        motionManager.startGyroUpdates(to: .main) { [weak self] data, _ in
            guard let rotationRate = data?.rotationRate else { return }
            Task { @MainActor [weak self] in
                self?.consume(x: rotationRate.x, y: rotationRate.y)
            }
        }
    }

    func stop() {
        motionManager.stopGyroUpdates()
        filteredRate = .zero
        onCameraMotion?(.zero)
        EmulatorBridge.shared.setRightStickMotion(x: 0, y: 0)
    }

    private func consume(x: Double, y: Double) {
        let raw = Self.screenRate(x: x, y: y, orientation: interfaceOrientation)
        let settings = DynamicThumbstickSettings.shared
        let retained = min(max(settings.gyroSmoothing, 0), 0.95)
        let blend = CGFloat(1 - retained)
        filteredRate = DynamicThumbstickVector(
            x: filteredRate.x + (raw.x - filteredRate.x) * blend,
            y: filteredRate.y + (raw.y - filteredRate.y) * blend
        )

        let processed = Self.processedRate(
            filteredRate,
            sensitivity: settings.gyroSensitivity,
            acceleration: settings.gyroAcceleration,
            deadZone: settings.gyroDeadZone,
            maximumRate: settings.gyroMaximumRate,
            invertHorizontal: settings.invertGyroHorizontal,
            invertVertical: settings.invertGyroVertical
        )
        let referenceRadiansPerSecond = CGFloat(105.0 * Double.pi / 180.0)
        let motion = DynamicThumbstickVector(
            x: processed.x / referenceRadiansPerSecond,
            y: processed.y / referenceRadiansPerSecond
        )
        onCameraMotion?(motion)
        EmulatorBridge.shared.setRightStickMotion(
            x: Float(motion.x),
            y: Float(motion.y)
        )
    }

    private var interfaceOrientation: UIInterfaceOrientation {
        UIApplication.shared.connectedScenes
            .compactMap { $0 as? UIWindowScene }
            .first(where: { $0.activationState == .foregroundActive })?
            .interfaceOrientation ?? .landscapeRight
    }

    private static func screenRate(x: Double, y: Double, orientation: UIInterfaceOrientation) -> DynamicThumbstickVector {
        switch orientation {
        case .landscapeLeft: return DynamicThumbstickVector(x: x, y: -y)
        case .landscapeRight: return DynamicThumbstickVector(x: -x, y: y)
        case .portraitUpsideDown: return DynamicThumbstickVector(x: y, y: x)
        default: return DynamicThumbstickVector(x: -y, y: -x)
        }
    }

    private static func processedRate(
        _ raw: DynamicThumbstickVector,
        sensitivity: Double,
        acceleration: Double,
        deadZone: Double,
        maximumRate: Double,
        invertHorizontal: Bool,
        invertVertical: Bool
    ) -> DynamicThumbstickVector {
        let magnitude = raw.magnitude
        let deadZone = CGFloat(max(deadZone, 0))
        guard magnitude > deadZone, magnitude > 0 else { return .zero }

        let limit = CGFloat(max(maximumRate, 0.01))
        let adjustedMagnitude = min(magnitude - deadZone, limit)
        let speed = min(adjustedMagnitude / limit, 1)
        let gain = 1 + CGFloat(max(acceleration, 0)) * speed
        let outputMagnitude = adjustedMagnitude * CGFloat(max(sensitivity, 0)) * gain
        return DynamicThumbstickVector(
            x: raw.x / magnitude * outputMagnitude * (invertHorizontal ? -1 : 1),
            y: raw.y / magnitude * outputMagnitude * (invertVertical ? -1 : 1)
        )
    }
}

@MainActor
private final class VirtualPadActionPressCoordinator {
    static let shared = VirtualPadActionPressCoordinator()

    private var pressedSources: [VirtualPadActionButton: Set<String>] = [:]

    func set(_ button: VirtualPadActionButton, source: String, pressed: Bool) {
        var sources = pressedSources[button, default: []]
        let wasPressed = !sources.isEmpty
        if pressed {
            sources.insert(source)
        } else {
            sources.remove(source)
        }

        if sources.isEmpty {
            pressedSources.removeValue(forKey: button)
        } else {
            pressedSources[button] = sources
        }

        let isPressed = !sources.isEmpty
        if wasPressed != isPressed {
            EmulatorBridge.shared.setPadButton(button.padButton, pressed: isPressed)
        }
    }
}

@MainActor
@Observable
final class DynamicCrosshairRuntimeState {
    private(set) var isAiming = false
    private(set) var isCameraMoving = false
    private(set) var isCameraSettling = false
    private(set) var isShooting = false
    private(set) var isRapidFiring = false
    private(set) var cameraMotion = DynamicThumbstickVector.zero
    private(set) var cameraAcceleration = DynamicThumbstickVector.zero
    private(set) var movementStartedAt = 0.0
    private(set) var movementEndedAt = 0.0
    private(set) var shotStartedAt = 0.0
    private(set) var shotTiltRadians: CGFloat = 0
    private(set) var rapidFireStartedAt = 0.0

    @ObservationIgnored private var sourceMotion: [DynamicCrosshairMotionSource: DynamicThumbstickVector] = [:]
    @ObservationIgnored private var settlingTask: Task<Void, Never>?
    @ObservationIgnored private var shotTask: Task<Void, Never>?

    func setAiming(_ aiming: Bool) {
        isAiming = aiming
        if !aiming {
            clearTransientActivity()
        }
    }

    func updateCameraMotion(_ motion: DynamicThumbstickVector, source: DynamicCrosshairMotionSource) {
        guard isAiming else { return }

        let limitedMotion = motion.limited(to: 1.35)
        if limitedMotion.magnitude > 0.003 {
            sourceMotion[source] = limitedMotion
        } else {
            sourceMotion.removeValue(forKey: source)
        }

        let combined = sourceMotion.values.reduce(DynamicThumbstickVector.zero) { partial, next in
            DynamicThumbstickVector(x: partial.x + next.x, y: partial.y + next.y)
        }
        let target = combined.limited(to: 1.35)
        let now = Date.timeIntervalSinceReferenceDate

        guard target.magnitude > 0.003 else {
            beginCameraSettle(at: now)
            return
        }

        if !isCameraMoving {
            movementStartedAt = now
        }
        if isCameraSettling {
            settlingTask?.cancel()
            settlingTask = nil
        }

        let previous = cameraMotion
        let response: CGFloat = isCameraMoving ? 0.52 : 0.72
        let filtered = DynamicThumbstickVector(
            x: previous.x + (target.x - previous.x) * response,
            y: previous.y + (target.y - previous.y) * response
        )
        cameraAcceleration = DynamicThumbstickVector(
            x: (filtered.x - previous.x) * 2.2,
            y: (filtered.y - previous.y) * 2.2
        ).limited(to: 1)
        cameraMotion = filtered
        isCameraMoving = true
        isCameraSettling = false
    }

    func triggerShot() {
        guard isAiming else { return }
        shotStartedAt = Date.timeIntervalSinceReferenceDate
        shotTiltRadians = .random(in: -0.07...0.07)
        isShooting = true
        shotTask?.cancel()
        shotTask = nil
        guard !isRapidFiring else { return }
        shotTask = Task { @MainActor [weak self] in
            try? await Task.sleep(for: .milliseconds(160))
            guard !Task.isCancelled else { return }
            self?.isShooting = false
        }
    }

    func setRapidFiring(_ firing: Bool) {
        guard isAiming || !firing else { return }
        if firing && !isRapidFiring {
            rapidFireStartedAt = Date.timeIntervalSinceReferenceDate
        }
        isRapidFiring = firing
        if !firing {
            shotTask?.cancel()
            shotTask = nil
            isShooting = false
        }
    }

    func reset() {
        isAiming = false
        clearTransientActivity()
    }

    private func clearTransientActivity() {
        settlingTask?.cancel()
        shotTask?.cancel()
        settlingTask = nil
        shotTask = nil
        sourceMotion.removeAll(keepingCapacity: true)
        isCameraMoving = false
        isCameraSettling = false
        isShooting = false
        isRapidFiring = false
        cameraMotion = .zero
        cameraAcceleration = .zero
        shotTiltRadians = 0
    }

    private func beginCameraSettle(at now: TimeInterval) {
        guard isCameraMoving else { return }
        isCameraMoving = false
        isCameraSettling = true
        movementEndedAt = now
        settlingTask?.cancel()
        settlingTask = Task { @MainActor [weak self] in
            try? await Task.sleep(for: .milliseconds(260))
            guard !Task.isCancelled, let self else { return }
            self.isCameraSettling = false
            self.cameraMotion = .zero
            self.cameraAcceleration = .zero
            self.settlingTask = nil
        }
    }
}

@MainActor
final class VirtualPadTouchActionController {
    private let side: VirtualPadThumbstickSide
    let crosshairState = DynamicCrosshairRuntimeState()
    private var lastInteractionTapTime: CFTimeInterval?
    private var lastFireTapTime: CFTimeInterval?
    private var fireTapCount = 0
    private var touchActive = false
    private var aimEngaged = false
    private var enteredAimThisInteraction = false
    private var rapidFireEngaged = false
    private var aimReleaseTask: Task<Void, Never>?
    private var fireReleaseTask: Task<Void, Never>?
    private var fireLoopTask: Task<Void, Never>?
    private var tapFireTask: Task<Void, Never>?
    private var pendingSingleFireTask: Task<Void, Never>?
    private var activeAimButton: VirtualPadActionButton?
    private var activeTapFireButton: VirtualPadActionButton?
    private var activeHoldFireButton: VirtualPadActionButton?

    init(side: VirtualPadThumbstickSide) {
        self.side = side
    }

    var isAiming: Bool { aimEngaged }

    func updateCameraMotion(_ motion: DynamicThumbstickVector, source: DynamicCrosshairMotionSource) {
        crosshairState.updateCameraMotion(motion, source: source)
    }

    func interactionBegan() {
        let settings = DynamicThumbstickSettings.shared
        let now = CACurrentMediaTime()
        touchActive = true
        enteredAimThisInteraction = false
        aimReleaseTask?.cancel()
        fireReleaseTask?.cancel()

        let isDoubleTap = settings.doubleTapToHoldAim &&
            !aimEngaged &&
            lastInteractionTapTime.map { now - $0 <= settings.doubleTapWindow } == true
        if !aimEngaged && (settings.holdAimWhileSwipe || isDoubleTap) {
            aimEngaged = true
            enteredAimThisInteraction = true
            cancelPendingSingleFire()
            resetFireTapSequence()
            setAimPressed(true)
        }

        let insideRapidWindow = lastFireTapTime.map { now - $0 <= settings.rapidTapWindow } == true
        if !insideRapidWindow { fireTapCount = 0 }
        if !enteredAimThisInteraction &&
            settings.rapidTapFireEnabled &&
            insideRapidWindow &&
            fireTapCount >= max(settings.rapidTapActivationCount - 1, 1) {
            startRapidFire()
        }
    }

    func interactionActivity() {
        guard touchActive else { return }
        if DynamicThumbstickSettings.shared.extendFireWhileDragging && rapidFireEngaged {
            fireReleaseTask?.cancel()
        }
    }

    func interactionTapped() {
        let settings = DynamicThumbstickSettings.shared
        let now = CACurrentMediaTime()
        lastInteractionTapTime = now

        // The touch that first engages aim is a mode-changing gesture, not a
        // shot. It also must not seed the rapid-fire recognizer.
        if enteredAimThisInteraction {
            enteredAimThisInteraction = false
            resetFireTapSequence()
            return
        }

        if let lastFireTapTime, now - lastFireTapTime <= settings.rapidTapWindow {
            fireTapCount += 1
        } else {
            fireTapCount = 1
        }
        self.lastFireTapTime = now

        if rapidFireEngaged {
            return
        }
        guard settings.tapToFire else { return }
        if settings.doubleTapToHoldAim && !aimEngaged {
            scheduleSingleFire(after: settings.doubleTapWindow)
        } else {
            pulseFire()
        }
    }

    func interactionEnded() {
        touchActive = false
        scheduleAimRelease()
        guard rapidFireEngaged else { return }
        if DynamicThumbstickSettings.shared.releaseFireWhenTouchEnds {
            stopRapidFire()
        } else {
            scheduleFireRelease()
        }
    }

    func reset() {
        aimReleaseTask?.cancel()
        fireReleaseTask?.cancel()
        fireLoopTask?.cancel()
        tapFireTask?.cancel()
        pendingSingleFireTask?.cancel()
        aimReleaseTask = nil
        fireReleaseTask = nil
        fireLoopTask = nil
        tapFireTask = nil
        pendingSingleFireTask = nil
        touchActive = false
        aimEngaged = false
        enteredAimThisInteraction = false
        rapidFireEngaged = false
        lastInteractionTapTime = nil
        resetFireTapSequence()
        setAimPressed(false)
        setTapFirePressed(false)
        setHoldFirePressed(false)
        crosshairState.reset()
    }

    private func scheduleAimRelease() {
        guard aimEngaged else { return }
        aimReleaseTask?.cancel()
        let delay = max(DynamicThumbstickSettings.shared.aimReleaseDelay, 0)
        aimReleaseTask = Task { @MainActor [weak self] in
            try? await Task.sleep(for: .seconds(delay))
            guard !Task.isCancelled, let self, !self.touchActive else { return }
            self.aimEngaged = false
            self.setAimPressed(false)
        }
    }

    private func scheduleFireRelease() {
        fireReleaseTask?.cancel()
        let delay = max(DynamicThumbstickSettings.shared.fireReleaseDelay, 0)
        fireReleaseTask = Task { @MainActor [weak self] in
            try? await Task.sleep(for: .seconds(delay))
            guard !Task.isCancelled, let self, !self.touchActive else { return }
            self.stopRapidFire()
        }
    }

    private func pulseFire() {
        tapFireTask?.cancel()
        setTapFirePressed(false)
        setTapFirePressed(true)
        crosshairState.triggerShot()
        HapticManager.dynamicActionShot()
        tapFireTask = Task { @MainActor [weak self] in
            try? await Task.sleep(for: .milliseconds(75))
            guard !Task.isCancelled else { return }
            self?.setTapFirePressed(false)
        }
    }

    private func scheduleSingleFire(after delay: TimeInterval) {
        pendingSingleFireTask?.cancel()
        pendingSingleFireTask = Task { @MainActor [weak self] in
            try? await Task.sleep(for: .seconds(max(delay, 0)))
            guard !Task.isCancelled, let self else { return }
            self.pendingSingleFireTask = nil
            self.pulseFire()
        }
    }

    private func cancelPendingSingleFire() {
        pendingSingleFireTask?.cancel()
        pendingSingleFireTask = nil
        tapFireTask?.cancel()
        tapFireTask = nil
        setTapFirePressed(false)
    }

    private func resetFireTapSequence() {
        lastFireTapTime = nil
        fireTapCount = 0
    }

    private func startRapidFire() {
        guard !rapidFireEngaged else { return }
        cancelPendingSingleFire()
        rapidFireEngaged = true
        crosshairState.setRapidFiring(true)
        HapticManager.dynamicActionRapidFire()
        fireLoopTask?.cancel()
        fireLoopTask = Task { @MainActor [weak self] in
            while !Task.isCancelled, let self, self.rapidFireEngaged {
                let interval = max(DynamicThumbstickSettings.shared.automaticFireInterval, 0.06)
                self.setHoldFirePressed(true)
                self.crosshairState.triggerShot()
                try? await Task.sleep(for: .seconds(min(interval * 0.45, 0.05)))
                if Task.isCancelled { break }
                self.setHoldFirePressed(false)
                try? await Task.sleep(for: .seconds(max(interval * 0.55, 0.02)))
            }
            self?.setHoldFirePressed(false)
        }
    }

    private func stopRapidFire() {
        rapidFireEngaged = false
        crosshairState.setRapidFiring(false)
        fireLoopTask?.cancel()
        fireLoopTask = nil
        setHoldFirePressed(false)
    }

    private func setAimPressed(_ pressed: Bool) {
        crosshairState.setAiming(pressed)
        Self.setActionButton(
            pressed: pressed,
            activeButton: &activeAimButton,
            selectedButton: DynamicThumbstickSettings.shared.aimButton(for: side),
            source: "\(sourcePrefix).aim"
        )
        if pressed {
            HapticManager.dynamicActionAim()
        }
    }

    private func setTapFirePressed(_ pressed: Bool) {
        Self.setActionButton(
            pressed: pressed,
            activeButton: &activeTapFireButton,
            selectedButton: DynamicThumbstickSettings.shared.fireButton(for: side),
            source: "\(sourcePrefix).tapFire"
        )
    }

    private func setHoldFirePressed(_ pressed: Bool) {
        Self.setActionButton(
            pressed: pressed,
            activeButton: &activeHoldFireButton,
            selectedButton: DynamicThumbstickSettings.shared.holdFireButton(for: side),
            source: "\(sourcePrefix).holdFire"
        )
    }

    private var sourcePrefix: String {
        side == .left ? "leftThumbstick" : "rightThumbstick"
    }

    private static func setActionButton(
        pressed: Bool,
        activeButton: inout VirtualPadActionButton?,
        selectedButton: VirtualPadActionButton,
        source: String
    ) {
        if pressed {
            if let previous = activeButton, previous != selectedButton {
                VirtualPadActionPressCoordinator.shared.set(previous, source: source, pressed: false)
            }
            activeButton = selectedButton
            VirtualPadActionPressCoordinator.shared.set(selectedButton, source: source, pressed: true)
        } else if let button = activeButton {
            VirtualPadActionPressCoordinator.shared.set(button, source: source, pressed: false)
            activeButton = nil
        }
    }
}

/// Shared ownership boundary for camera-touch actions and their crosshair
/// runtime. GameScreenView keeps this session so the crosshair can render in
/// the Metal viewport while VirtualControllerView continues driving input.
@MainActor
final class VirtualPadTouchActionSession {
    let left = VirtualPadTouchActionController(side: .left)
    let right = VirtualPadTouchActionController(side: .right)

    func reset() {
        left.reset()
        right.reset()
    }
}

struct DynamicAimCrosshairOverlay: View {
    let settings: DynamicThumbstickSettings
    let leftRuntime: DynamicCrosshairRuntimeState
    let rightRuntime: DynamicCrosshairRuntimeState

    private var activeRuntime: DynamicCrosshairRuntimeState? {
        if rightRuntime.isAiming { return rightRuntime }
        if leftRuntime.isAiming { return leftRuntime }
        return nil
    }

    var body: some View {
        ZStack {
            if settings.dynamicCrosshairEnabled, let activeRuntime {
                DynamicAimCrosshairView(
                    type: settings.dynamicCrosshairType,
                    animation: settings.dynamicCrosshairAnimation,
                    configuredSize: CGFloat(settings.dynamicCrosshairSize),
                    configuredOpacity: CGFloat(settings.dynamicCrosshairOpacity),
                    runtime: activeRuntime
                )
                .transition(.scale(scale: 0.76).combined(with: .opacity))
            }
        }
        .animation(
            .spring(response: 0.22, dampingFraction: 0.78),
            value: settings.dynamicCrosshairEnabled && activeRuntime != nil
        )
        .allowsHitTesting(false)
        .accessibilityHidden(true)
    }
}

private struct DynamicAimCrosshairView: View {
    let type: DynamicCrosshairType
    let animation: DynamicCrosshairAnimation
    let configuredSize: CGFloat
    let configuredOpacity: CGFloat
    let runtime: DynamicCrosshairRuntimeState

    var body: some View {
        let isAnimating = runtime.isCameraMoving ||
            runtime.isCameraSettling ||
            runtime.isShooting ||
            runtime.isRapidFiring
        TimelineView(.animation(minimumInterval: 1.0 / 60.0, paused: !isAnimating)) { timeline in
            Canvas(rendersAsynchronously: true) { context, canvasSize in
                let metrics = animationMetrics(at: timeline.date.timeIntervalSinceReferenceDate)
                var transformed = context
                transformed.translateBy(
                    x: canvasSize.width / 2 + metrics.offset.width,
                    y: canvasSize.height / 2 + metrics.offset.height
                )
                transformed.rotate(by: .radians(Double(metrics.rotation)))
                transformed.scaleBy(
                    x: metrics.scale * metrics.scaleX,
                    y: metrics.scale * metrics.scaleY
                )
                drawCrosshair(
                    in: &transformed,
                    radius: configuredSize / 2,
                    spread: metrics.spread,
                    opacity: metrics.opacity * min(max(configuredOpacity, 0), 1),
                    movementReaction: metrics.movementReaction,
                    shotReaction: metrics.shotReaction,
                    rapidFireReaction: metrics.rapidFireReaction,
                    motion: metrics.motion
                )
            }
        }
        .frame(width: configuredSize * 2.2, height: configuredSize * 2.2)
    }

    private func animationMetrics(at time: TimeInterval) -> CrosshairAnimationMetrics {
        let moving = runtime.isCameraMoving
        let settling = runtime.isCameraSettling
        let shooting = runtime.isShooting
        let rapid = runtime.isRapidFiring
        let movementPhase = max(time - runtime.movementStartedAt, 0)
        let settlingProgress = settling
            ? min(max((time - runtime.movementEndedAt) / 0.26, 0), 1)
            : 0
        let movementDecay: CGFloat = moving ? 1 : (settling ? CGFloat(1 - settlingProgress) : 0)
        let motion = DynamicThumbstickVector(
            x: runtime.cameraMotion.x * movementDecay,
            y: runtime.cameraMotion.y * movementDecay
        )
        let acceleration = DynamicThumbstickVector(
            x: runtime.cameraAcceleration.x * movementDecay,
            y: runtime.cameraAcceleration.y * movementDecay
        )
        let speed = min(motion.magnitude, 1.25)
        let accelerationStrength = min(acceleration.magnitude, 1)
        let normalizedSpeed = min(speed, 1)
        let direction = atan2(Double(motion.y), Double(motion.x))
        let shotProgress = shooting ? min(max((time - runtime.shotStartedAt) / 0.16, 0), 1) : 1
        let shotImpulse = shooting ? CGFloat(1 - shotProgress) : 0
        let rapidPhase = max(time - runtime.rapidFireStartedAt, 0)
        let liveFrequency = 8 + Double(normalizedSpeed) * 14
        let moveWave = movementDecay * CGFloat(sin(movementPhase * liveFrequency))
        let rapidWave = rapid ? CGFloat(sin(rapidPhase * 34)) : 0
        let perpendicular = DynamicThumbstickVector(x: -motion.y, y: motion.x)

        var result = CrosshairAnimationMetrics()
        switch animation {
        case .reactive:
            result.spread = 1 + speed * 0.20 + moveWave * 0.025 + accelerationStrength * 0.05 + shotImpulse * 0.30
            result.scale = 1 - shotImpulse * 0.10 + (rapid ? rapidWave * 0.045 : 0)
            result.offset = CGSize(
                width: motion.x * configuredSize * 0.030,
                height: motion.y * configuredSize * 0.030
            )
        case .pulse:
            result.scale = 1 + moveWave * (0.025 + speed * 0.06) + speed * 0.025 + shotImpulse * 0.22 +
                (rapid ? rapidWave * 0.10 : 0)
            result.opacity = 1 - abs(moveWave) * speed * 0.08 - (rapid ? abs(rapidWave) * 0.08 : 0)
            result.offset = CGSize(
                width: motion.x * configuredSize * 0.018,
                height: motion.y * configuredSize * 0.018
            )
        case .expand:
            result.spread = 1 + speed * 0.34 + abs(moveWave) * speed * 0.09 + shotImpulse * 0.48
            if rapid { result.spread += 0.14 + abs(rapidWave) * 0.16 }
            result.scaleX = 1 + abs(motion.x) * 0.09
            result.scaleY = 1 + abs(motion.y) * 0.09
        case .rotate:
            result.rotation = CGFloat(movementPhase * (0.45 + Double(speed) * 1.1)) * movementDecay
            result.spread = 1 + speed * 0.14 + shotImpulse * 0.28
            result.scale = 1 + shotImpulse * 0.08
        case .recoil:
            result.offset.height = motion.y * configuredSize * 0.042 - configuredSize * shotImpulse * 0.16
            result.offset.width = motion.x * configuredSize * 0.042 +
                (rapid ? configuredSize * rapidWave * 0.055 : 0)
            result.spread = 1 + speed * 0.14 + shotImpulse * 0.20
        case .orbit:
            let phase = rapid ? rapidPhase * 7 : movementPhase * (2.5 + Double(speed) * 3)
            let activity: CGFloat = rapid ? 1 : speed
            result.offset = CGSize(
                width: motion.x * configuredSize * 0.035 +
                    CGFloat(cos(phase + direction)) * configuredSize * 0.055 * activity,
                height: motion.y * configuredSize * 0.035 +
                    CGFloat(sin(phase + direction)) * configuredSize * 0.055 * activity
            )
            result.spread = 1 + shotImpulse * 0.32
        case .focus:
            result.spread = 0.90 + speed * 0.36 + abs(moveWave) * speed * 0.05
            if shooting { result.spread -= shotImpulse * 0.20 }
            if rapid { result.spread += rapidWave * 0.08 }
            result.scale = 1 + shotImpulse * 0.08
            result.offset = CGSize(
                width: -motion.x * configuredSize * 0.020,
                height: -motion.y * configuredSize * 0.020
            )
        case .wave:
            result.scale = 1 + moveWave * speed * 0.075 + (rapid ? rapidWave * 0.08 : 0)
            result.spread = 1 + speed * 0.10 + shotImpulse * 0.36
            result.opacity = 1 - CGFloat(abs(rapidWave)) * 0.10
            result.offset = CGSize(
                width: perpendicular.x * configuredSize * moveWave * 0.035,
                height: perpendicular.y * configuredSize * moveWave * 0.035
            )
        case .directional:
            result.offset = CGSize(
                width: motion.x * configuredSize * 0.085,
                height: motion.y * configuredSize * 0.085
            )
            result.scaleX = 1 + abs(motion.x) * 0.14
            result.scaleY = 1 + abs(motion.y) * 0.14
            result.spread = 1 + speed * 0.13 + shotImpulse * 0.30
        case .elastic:
            result.offset = CGSize(
                width: (-motion.x * 0.075 + acceleration.x * 0.055) * configuredSize,
                height: (-motion.y * 0.075 + acceleration.y * 0.055) * configuredSize
            )
            result.scaleX = 1 + abs(motion.x) * 0.10 - abs(motion.y) * 0.025
            result.scaleY = 1 + abs(motion.y) * 0.10 - abs(motion.x) * 0.025
            result.spread = 1 + speed * 0.12 + accelerationStrength * 0.12 + shotImpulse * 0.28
        case .parallax:
            result.offset = CGSize(
                width: -motion.x * configuredSize * 0.095,
                height: -motion.y * configuredSize * 0.095
            )
            result.scaleX = 1 + abs(motion.y) * 0.045
            result.scaleY = 1 + abs(motion.x) * 0.045
            result.spread = 1 + speed * 0.18 + shotImpulse * 0.30
        case .velocity:
            result.spread = 1 + speed * 0.52 + accelerationStrength * 0.08 + shotImpulse * 0.35
            result.scale = 1 + speed * 0.055 - shotImpulse * 0.08
            result.scaleX = 1 + abs(motion.x) * 0.12
            result.scaleY = 1 + abs(motion.y) * 0.12
            result.offset = CGSize(
                width: motion.x * configuredSize * 0.025,
                height: motion.y * configuredSize * 0.025
            )
        case .stabilizer:
            result.offset = CGSize(
                width: -motion.x * configuredSize * 0.042,
                height: -motion.y * configuredSize * 0.042
            )
            result.spread = 1 - speed * 0.10 + accelerationStrength * 0.04 + shotImpulse * 0.26
            result.scaleX = 1 + abs(motion.y) * 0.025
            result.scaleY = 1 + abs(motion.x) * 0.025
        case .snap:
            result.offset = CGSize(
                width: acceleration.x * configuredSize * 0.095,
                height: acceleration.y * configuredSize * 0.095
            )
            result.spread = 1 + speed * 0.10 + accelerationStrength * 0.30 + shotImpulse * 0.34
            result.scale = 1 + accelerationStrength * 0.08
        case .drift:
            result.offset = CGSize(
                width: (motion.x * 0.050 + perpendicular.x * moveWave * 0.030) * configuredSize,
                height: (motion.y * 0.050 + perpendicular.y * moveWave * 0.030) * configuredSize
            )
            result.spread = 1 + speed * 0.15 + shotImpulse * 0.28
        case .tilt:
            result.rotation = runtime.shotTiltRadians * shotImpulse * 1.35
            result.offset = CGSize(
                width: motion.x * configuredSize * 0.025,
                height: motion.y * configuredSize * 0.025
            )
            result.spread = 1 + speed * 0.16 + shotImpulse * 0.30
        case .bloom:
            result.spread = 1 + speed * 0.42 + abs(moveWave) * speed * 0.10 + shotImpulse * 0.45
            result.scale = 1 + speed * 0.07 + moveWave * speed * 0.035
            result.opacity = 1 - speed * 0.12 + abs(moveWave) * speed * 0.04
            result.scaleX = 1 + abs(motion.x) * 0.08
            result.scaleY = 1 + abs(motion.y) * 0.08
        case .directionLock:
            result.offset = CGSize(
                width: motion.x * configuredSize * 0.045,
                height: motion.y * configuredSize * 0.045
            )
            result.scaleY = 1 + speed * 0.18
            result.scaleX = 1 - min(speed * 0.045, 0.045)
            result.spread = 1 + speed * 0.20 + shotImpulse * 0.32
        }

        // Every type receives a small live deformation even when its selected
        // animation emphasizes only one axis or behavior.
        result.scaleX *= 1 + abs(motion.x) * 0.025
        result.scaleY *= 1 + abs(motion.y) * 0.025
        result.offset.width += motion.x * configuredSize * 0.008
        result.offset.height += motion.y * configuredSize * 0.008
        if animation != .tilt {
            result.rotation += runtime.shotTiltRadians * shotImpulse
        }
        result.movementReaction = min(speed + accelerationStrength * 0.22, 1.35)
        result.shotReaction = shotImpulse
        result.rapidFireReaction = rapid ? 1 + rapidWave * 0.12 : 0
        result.motion = motion
        return result
    }

    private func drawCrosshair(
        in context: inout GraphicsContext,
        radius: CGFloat,
        spread: CGFloat,
        opacity: CGFloat,
        movementReaction: CGFloat,
        shotReaction: CGFloat,
        rapidFireReaction: CGFloat,
        motion: DynamicThumbstickVector
    ) {
        let lineWidth = max(radius / 28, 0.75)
        let gap = radius * 0.27 * spread
        let armEnd = radius * min(max(spread, 0.72), 1.55)
        let fireReaction = max(shotReaction, rapidFireReaction)
        let isFiring = fireReaction > 0
        let reactiveColour = isFiring ? Color(red: 1, green: 0.16, blue: 0.10) : .white
        let reactiveOpacity = isFiring ? min(opacity, 0.40) : opacity

        switch type {
        case .classic:
            stroke(
                directionalRadialLines(
                    count: 4,
                    innerRadius: gap,
                    outerRadius: armEnd,
                    motion: motion,
                    reaction: movementReaction,
                    radius: radius
                ),
                in: &context,
                lineWidth: lineWidth,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
        case .dot:
            motionTrailDots(
                motion: motion,
                radius: radius,
                reaction: movementReaction,
                in: &context,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
            dot(
                at: .zero,
                radius: max(radius * 0.075, 1.25),
                in: &context,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
        case .circle:
            stroke(
                circle(radius: radius * 0.52 * spread),
                in: &context,
                lineWidth: lineWidth,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
        case .circleDot:
            stroke(
                circle(radius: radius * 0.52 * spread),
                in: &context,
                lineWidth: lineWidth,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
            motionTrailDots(
                motion: motion,
                radius: radius,
                reaction: movementReaction,
                in: &context,
                opacity: reactiveOpacity * 0.75,
                foreground: reactiveColour
            )
            dot(
                at: .zero,
                radius: max(radius * 0.06, 1.1),
                in: &context,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
        case .cross:
            stroke(
                directionalRadialLines(
                    count: 4,
                    innerRadius: 0,
                    outerRadius: armEnd,
                    motion: motion,
                    reaction: movementReaction,
                    radius: radius
                ),
                in: &context,
                lineWidth: lineWidth,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
        case .chevron:
            var path = Path()
            path.move(to: CGPoint(x: -radius * 0.54 * spread, y: radius * 0.28))
            path.addLine(to: CGPoint(x: 0, y: -radius * 0.24 * spread))
            path.addLine(to: CGPoint(x: radius * 0.54 * spread, y: radius * 0.28))
            stroke(
                path,
                in: &context,
                lineWidth: lineWidth,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
        case .brackets:
            stroke(
                cornerBrackets(radius: radius * spread),
                in: &context,
                lineWidth: lineWidth,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
        case .diamond:
            var path = Path()
            path.move(to: CGPoint(x: 0, y: -radius * 0.62 * spread))
            path.addLine(to: CGPoint(x: radius * 0.62 * spread, y: 0))
            path.addLine(to: CGPoint(x: 0, y: radius * 0.62 * spread))
            path.addLine(to: CGPoint(x: -radius * 0.62 * spread, y: 0))
            path.closeSubpath()
            stroke(
                path,
                in: &context,
                lineWidth: lineWidth,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
        case .shotgun:
            stroke(
                circle(radius: radius * 0.48 * spread),
                in: &context,
                lineWidth: lineWidth,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
            for index in 0..<6 {
                let angle = Double(index) * .pi / 3
                dot(
                    at: CGPoint(
                        x: CGFloat(cos(angle)) * radius * 0.72 * spread,
                        y: CGFloat(sin(angle)) * radius * 0.72 * spread
                    ),
                    radius: max(radius * 0.045, 1),
                    in: &context,
                    opacity: reactiveOpacity,
                    foreground: reactiveColour
                )
            }
        case .sniper:
            stroke(
                circle(radius: radius * 0.56 * spread),
                in: &context,
                lineWidth: lineWidth,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
            stroke(
                directionalRadialLines(
                    count: 4,
                    innerRadius: radius * 0.68 * spread,
                    outerRadius: armEnd,
                    motion: motion,
                    reaction: movementReaction,
                    radius: radius
                ),
                in: &context,
                lineWidth: lineWidth,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
            dot(
                at: .zero,
                radius: max(radius * 0.05, 1),
                in: &context,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
        case .tactical:
            stroke(
                directionalRadialLines(
                    count: 4,
                    innerRadius: gap * 1.08,
                    outerRadius: armEnd * 0.88,
                    motion: motion,
                    reaction: movementReaction,
                    radius: radius
                ),
                in: &context,
                lineWidth: lineWidth,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
            dot(
                at: .zero,
                radius: max(radius * 0.055, 1.1),
                in: &context,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
            var marker = Path()
            marker.move(to: CGPoint(x: 0, y: -radius * 0.98 * spread))
            marker.addLine(to: CGPoint(x: -radius * 0.11, y: -radius * 0.80 * spread))
            marker.addLine(to: CGPoint(x: radius * 0.11, y: -radius * 0.80 * spread))
            marker.closeSubpath()
            fill(
                marker,
                in: &context,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
        case .burst:
            stroke(
                directionalRadialLines(
                    count: 8,
                    innerRadius: gap,
                    outerRadius: armEnd,
                    motion: motion,
                    reaction: movementReaction,
                    radius: radius
                ),
                in: &context,
                lineWidth: lineWidth,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
        case .fourBoxes:
            let boxDistance = radius * (
                0.52 +
                0.13 * movementReaction +
                0.24 * fireReaction
            )
            let boxHalfSide = max(radius * 0.105, 1.25)
            for point in [
                CGPoint(x: 0, y: -boxDistance),
                CGPoint(x: boxDistance, y: 0),
                CGPoint(x: 0, y: boxDistance),
                CGPoint(x: -boxDistance, y: 0)
            ] {
                square(
                    at: point,
                    halfSide: boxHalfSide,
                    in: &context,
                    opacity: reactiveOpacity,
                    foreground: reactiveColour
                )
            }

            let restingLineLength = radius * 0.28
            let lineLength = restingLineLength +
                radius * 0.26 * movementReaction +
                radius * 0.24 * abs(motion.x) +
                radius * 0.18 * fireReaction
            let lineInner = boxDistance + boxHalfSide + radius * 0.14
            var horizontalLines = Path()
            horizontalLines.move(to: CGPoint(x: lineInner, y: 0))
            horizontalLines.addLine(to: CGPoint(x: lineInner + lineLength, y: 0))
            horizontalLines.move(to: CGPoint(x: -lineInner, y: 0))
            horizontalLines.addLine(to: CGPoint(x: -(lineInner + lineLength), y: 0))
            stroke(
                horizontalLines,
                in: &context,
                lineWidth: lineWidth,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
        case .triad:
            let outward = 1 + movementReaction * 0.20 + fireReaction * 0.36
            let baseSegmentCentre = radius * 0.58 * outward
            let segmentHalfLength = radius * (
                0.12 +
                0.08 * movementReaction +
                0.06 * fireReaction
            )
            var segments = Path()
            for angle in [-Double.pi / 6, -5 * Double.pi / 6, Double.pi / 2] {
                let direction = CGPoint(x: CGFloat(cos(angle)), y: CGFloat(sin(angle)))
                let directionalPush = (direction.x * motion.x + direction.y * motion.y) * radius * 0.13
                let segmentCentre = baseSegmentCentre + directionalPush
                segments.move(to: CGPoint(
                    x: direction.x * (segmentCentre - segmentHalfLength),
                    y: direction.y * (segmentCentre - segmentHalfLength)
                ))
                segments.addLine(to: CGPoint(
                    x: direction.x * (segmentCentre + segmentHalfLength),
                    y: direction.y * (segmentCentre + segmentHalfLength)
                ))
            }
            stroke(
                segments,
                in: &context,
                lineWidth: lineWidth,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
        case .reactiveDot:
            let dotScale = 1 + movementReaction * 0.08 + fireReaction * 0.30
            motionTrailDots(
                motion: motion,
                radius: radius,
                reaction: movementReaction,
                in: &context,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
            dot(
                at: .zero,
                radius: max(radius * 0.055, 0.95) * dotScale,
                in: &context,
                opacity: reactiveOpacity,
                foreground: reactiveColour
            )
        }
    }

    private func directionalRadialLines(
        count: Int,
        innerRadius: CGFloat,
        outerRadius: CGFloat,
        motion: DynamicThumbstickVector,
        reaction: CGFloat,
        radius: CGFloat
    ) -> Path {
        var path = Path()
        for index in 0..<count {
            let angle = Double(index) * 2 * .pi / Double(count)
            let direction = CGPoint(x: CGFloat(cos(angle)), y: CGFloat(sin(angle)))
            let projection = direction.x * motion.x + direction.y * motion.y
            let directionalExtension = projection * radius * 0.14 * reaction
            let directionalGap = max(projection, 0) * radius * 0.025 * reaction
            path.move(to: CGPoint(
                x: direction.x * (innerRadius + directionalGap),
                y: direction.y * (innerRadius + directionalGap)
            ))
            path.addLine(to: CGPoint(
                x: direction.x * max(outerRadius + directionalExtension, innerRadius + 1),
                y: direction.y * max(outerRadius + directionalExtension, innerRadius + 1)
            ))
        }
        return path
    }

    private func motionTrailDots(
        motion: DynamicThumbstickVector,
        radius: CGFloat,
        reaction: CGFloat,
        in context: inout GraphicsContext,
        opacity: CGFloat,
        foreground: Color = .white
    ) {
        guard reaction > 0.015 else { return }
        for index in 1...2 {
            let progress = CGFloat(index) / 2
            dot(
                at: CGPoint(
                    x: -motion.x * radius * 0.34 * progress,
                    y: -motion.y * radius * 0.34 * progress
                ),
                radius: max(radius * 0.045 * (1 - progress * 0.34), 0.7),
                in: &context,
                opacity: opacity * (0.34 - progress * 0.12),
                foreground: foreground
            )
        }
    }

    private func radialLines(count: Int, innerRadius: CGFloat, outerRadius: CGFloat) -> Path {
        var path = Path()
        for index in 0..<count {
            let angle = Double(index) * 2 * .pi / Double(count)
            path.move(to: CGPoint(x: CGFloat(cos(angle)) * innerRadius, y: CGFloat(sin(angle)) * innerRadius))
            path.addLine(to: CGPoint(x: CGFloat(cos(angle)) * outerRadius, y: CGFloat(sin(angle)) * outerRadius))
        }
        return path
    }

    private func cornerBrackets(radius: CGFloat) -> Path {
        let inner = radius * 0.38
        let outer = radius * 0.72
        var path = Path()
        for x: CGFloat in [-1, 1] {
            for y: CGFloat in [-1, 1] {
                path.move(to: CGPoint(x: x * inner, y: y * outer))
                path.addLine(to: CGPoint(x: x * outer, y: y * outer))
                path.addLine(to: CGPoint(x: x * outer, y: y * inner))
            }
        }
        return path
    }

    private func circle(radius: CGFloat) -> Path {
        Path(ellipseIn: CGRect(x: -radius, y: -radius, width: radius * 2, height: radius * 2))
    }

    private func stroke(
        _ path: Path,
        in context: inout GraphicsContext,
        lineWidth: CGFloat,
        opacity: CGFloat,
        foreground: Color = .white
    ) {
        context.stroke(path, with: .color(.black.opacity(0.72 * Double(opacity))), lineWidth: lineWidth + 1.15)
        context.stroke(path, with: .color(foreground.opacity(0.96 * Double(opacity))), lineWidth: lineWidth)
    }

    private func dot(
        at center: CGPoint,
        radius: CGFloat,
        in context: inout GraphicsContext,
        opacity: CGFloat,
        foreground: Color = .white
    ) {
        let rect = CGRect(
            x: center.x - radius,
            y: center.y - radius,
            width: radius * 2,
            height: radius * 2
        )
        context.fill(
            Path(ellipseIn: rect.insetBy(dx: -0.65, dy: -0.65)),
            with: .color(.black.opacity(0.72 * Double(opacity)))
        )
        context.fill(Path(ellipseIn: rect), with: .color(foreground.opacity(0.96 * Double(opacity))))
    }

    private func square(
        at center: CGPoint,
        halfSide: CGFloat,
        in context: inout GraphicsContext,
        opacity: CGFloat,
        foreground: Color
    ) {
        let rect = CGRect(
            x: center.x - halfSide,
            y: center.y - halfSide,
            width: halfSide * 2,
            height: halfSide * 2
        )
        context.fill(
            Path(rect.insetBy(dx: -0.65, dy: -0.65)),
            with: .color(.black.opacity(0.72 * Double(opacity)))
        )
        context.fill(Path(rect), with: .color(foreground.opacity(0.96 * Double(opacity))))
    }

    private func fill(
        _ path: Path,
        in context: inout GraphicsContext,
        opacity: CGFloat,
        foreground: Color = .white
    ) {
        context.stroke(path, with: .color(.black.opacity(0.72 * Double(opacity))), lineWidth: 1.15)
        context.fill(path, with: .color(foreground.opacity(0.96 * Double(opacity))))
    }
}

private struct CrosshairAnimationMetrics {
    var scale: CGFloat = 1
    var scaleX: CGFloat = 1
    var scaleY: CGFloat = 1
    var spread: CGFloat = 1
    var offset = CGSize.zero
    var rotation: CGFloat = 0
    var opacity: CGFloat = 1
    var movementReaction: CGFloat = 0
    var shotReaction: CGFloat = 0
    var rapidFireReaction: CGFloat = 0
    var motion = DynamicThumbstickVector.zero
}
