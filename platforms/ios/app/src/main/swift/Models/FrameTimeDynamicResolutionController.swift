// FrameTimeDynamicResolutionController.swift — opt-in adaptive internal resolution driven by frame-time history
// SPDX-License-Identifier: GPL-3.0+

import Foundation

/// Bumps `upscale_multiplier` in 0.25 steps based on the core's frame-time
/// history. Polls roughly every 500 ms and changes no more often than that.
///
/// Floor is 1.0 (native PS2). The ceiling is the UpscaleMultiplier the user
/// had set when adaptive was enabled; it only rises if the user raises
/// UpscaleMultiplier manually, never on its own. Hysteresis over the last
/// ~60 samples: above 22 ms drops a step, below 14 ms raises one.
///
/// Writes go through ARMSX2Bridge.setINIFloat (clamped, applied on the GS
/// thread). A per-game INI value overrides the global toggle for the running
/// game.
@MainActor
@Observable
final class FrameTimeDynamicResolutionController {
    static let shared = FrameTimeDynamicResolutionController()

    /// Drives the timer start/stop lifecycle. Bound to
    /// `SettingsStore.adaptiveResolutionEnabled`; the per-game override is
    /// read inside `poll()`.
    var enabled: Bool = false

    // MARK: - Tuning

    /// Polling interval (~30 frames at 60 Hz). Kept well above the 120 ms
    /// graphics-apply debounce so the timer can't fight a settings reload.
    private static let pollInterval: TimeInterval = 0.5
    /// Minimum interval between resolution changes, to avoid texture-cache
    /// thrash and GS pipeline stalls.
    private static let minChangeInterval: TimeInterval = 0.5
    private static let step: Float = 0.25
    /// Min clamp — never below native PS2.
    private static let minMultiplier: Float = 1.0
    /// Smoothing window — last 60 samples (~1 second at 60 Hz).
    private static let smoothingWindow: Int = 60
    private static let reduceThresholdMs: Float = 22.0
    private static let increaseThresholdMs: Float = 14.0
    /// Minimum non-zero sample count required to act. The history is empty
    /// until the first frames land, so the controller waits for enough data.
    private static let minSamplesForAction: Int = 60
    /// Window after one of our own writes during which an observed change is
    /// treated as our echo rather than a manual user change.
    private static let ownWriteEchoWindow: TimeInterval = 1.0
    /// A change larger than this multiple of `step` between two polls (outside
    /// the echo window) is treated as a manual user change.
    private static let manualChangeStepMultiple: Float = 1.5
    private static let perGameSection = "ARMSX2iOS/FramePacing"
    private static let perGameKey = "DynamicResolution"

    // MARK: - Internal state (not part of the observable surface)

    /// Max clamp — captured from the current UpscaleMultiplier on enable, and
    /// raised if the user raises it manually; never lowered otherwise.
    @ObservationIgnored private var maxMultiplier: Float = 1.0
    @ObservationIgnored private var lastChangeAt: Date = .distantPast
    @ObservationIgnored private var lastObservedMultiplier: Float = 1.0
    /// Used by the manual-change detector: if the observed value moves by
    /// more than a step from this and we didn't write recently, the user
    /// changed it.
    @ObservationIgnored private var lastWrittenValue: Float = 1.0
    @ObservationIgnored private var lastWriteAt: Date = .distantPast
    @ObservationIgnored private var pollTimer: Timer?
    @ObservationIgnored private var suspendedForEmulationOnlyMode = false

    private init() {}

    // MARK: - Lifecycle

    /// Called from `SettingsStore.adaptiveResolutionEnabled.didSet` on toggle,
    /// and after init once the persisted value has been read back. Idempotent.
    func setEnabled(_ newValue: Bool) {
        guard newValue != enabled else {
            if enabled && !suspendedForEmulationOnlyMode {
                startTimer()
            }
            return
        }
        enabled = newValue
        if enabled {
            // Capture the user's current UpscaleMultiplier as the max clamp.
            // The controller only rescues from drops below it.
            let current = SettingsStore.shared.upscaleMultiplier
            maxMultiplier = max(Self.minMultiplier, current)
            lastObservedMultiplier = current
            lastWrittenValue = current
            // Allow an immediate change on the first qualifying poll instead
            // of forcing a 500 ms wait after toggling.
            lastChangeAt = .distantPast
            lastWriteAt = .distantPast
            if !suspendedForEmulationOnlyMode {
                startTimer()
            }
        } else {
            stopTimer()
        }
    }

    /// Stops the optional 500 ms frame-history poll without changing the
    /// persisted Dynamic Resolution preference or the emulator's essential
    /// frame limiter/audio-video timing.
    func suspendForEmulationOnlyMode() {
        suspendedForEmulationOnlyMode = true
        stopTimer()
    }

    /// A new full gameplay presentation owns the optional monitor again.
    /// Idempotent, so normal sessions pay no extra timer or observer cost.
    func resumeAfterEmulationOnlyMode() {
        guard suspendedForEmulationOnlyMode else { return }
        suspendedForEmulationOnlyMode = false
        if enabled {
            startTimer()
        }
    }

    private func startTimer() {
        guard pollTimer == nil else { return }
        // Timer + .common run-loop mode so the tick keeps firing during
        // tracking (e.g. a pad drag). The Task hop preserves actor isolation.
        let timer = Timer(timeInterval: Self.pollInterval, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.poll() }
        }
        RunLoop.main.add(timer, forMode: .common)
        pollTimer = timer
    }

    private func stopTimer() {
        pollTimer?.invalidate()
        pollTimer = nil
    }

    deinit {
        MainActor.assumeIsolated {
            pollTimer?.invalidate()
        }
    }

    // MARK: - Poll

    private func poll() {
        // A per-game value, when present, overrides the global enable for the
        // running game: absent uses the global setting, 0 forces off, 1 on.
        if ARMSX2Bridge.hasPerGameINIValueForCurrentGame(Self.perGameSection, key: Self.perGameKey) {
            let perGame = ARMSX2Bridge.getPerGameINIBoolForCurrentGame(
                Self.perGameSection,
                key: Self.perGameKey,
                defaultValue: enabled
            )
            guard perGame else { return }
        }

        let history: [NSNumber] = ARMSX2Bridge.frameTimeHistory()
        let cursor: Int = Int(ARMSX2Bridge.frameTimeHistoryPos())
        let total: Int = history.count
        guard total > 0 else { return }

        // Collect the most recent `smoothingWindow` non-zero samples before
        // the cursor, wrapping around the ring buffer.
        var samples: [Float] = []
        samples.reserveCapacity(Self.smoothingWindow)
        for i in 0..<Self.smoothingWindow {
            let idx = ((cursor - 1 - i) % total + total) % total
            let value = history[idx].floatValue
            if value > 0 { samples.append(value) }
        }
        guard samples.count >= Self.minSamplesForAction else { return }

        let smoothed: Float = samples.reduce(0, +) / Float(samples.count)

        // Manual-change tracking: if UpscaleMultiplier moved by more than a
        // step-and-a-half from our last write and we didn't write recently,
        // the user changed it — adopt the new value as the ceiling.
        let currentMultiplier = SettingsStore.shared.upscaleMultiplier
        let now = Date()
        let insideOwnEchoWindow = now.timeIntervalSince(lastWriteAt) < Self.ownWriteEchoWindow
        let movedByMoreThanOneStep =
            abs(currentMultiplier - lastWrittenValue) > Self.step * Self.manualChangeStepMultiple
        if movedByMoreThanOneStep && !insideOwnEchoWindow {
            maxMultiplier = max(maxMultiplier, currentMultiplier)
        }
        lastObservedMultiplier = currentMultiplier

        let canChange = now.timeIntervalSince(lastChangeAt) >= Self.minChangeInterval

        // Hysteresis: above 22 ms drop one step, below 14 ms raise one step.
        if smoothed > Self.reduceThresholdMs,
           currentMultiplier > Self.minMultiplier,
           canChange {
            let newValue = max(Self.minMultiplier, currentMultiplier - Self.step)
            guard newValue != currentMultiplier else { return }
            commitChange(newValue: newValue, now: now)
        } else if smoothed < Self.increaseThresholdMs,
                  currentMultiplier < maxMultiplier,
                  canChange {
            let newValue = min(maxMultiplier, currentMultiplier + Self.step)
            guard newValue != currentMultiplier else { return }
            commitChange(newValue: newValue, now: now)
        }
    }

    /// Writes the new multiplier through the bridge and mirrors it into
    /// SettingsStore so the Graphics tab and Q-Menu track the change live.
    /// The SettingsStore didSet re-writes the same INI key (idempotent) and
    /// requests a debounced graphics-apply, which coalesces the GS reload.
    private func commitChange(newValue: Float, now: Date) {
        ARMSX2Bridge.setINIFloat("EmuCore/GS", key: "upscale_multiplier", value: newValue)
        SettingsStore.shared.upscaleMultiplier = newValue
        lastWrittenValue = newValue
        lastObservedMultiplier = newValue
        lastChangeAt = now
        lastWriteAt = now
    }
}
