// TransientBannerController.swift — auto-dismissing banner state machine
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

@MainActor
final class TransientBannerController<Value: Equatable>: ObservableObject {
    @Published var content: Value? = nil
    @Published private(set) var generation: Int = 0
    private var dismissTask: Task<Void, Never>? = nil
    private let defaultDisplayDuration: TimeInterval
    // Values arriving while a banner is already showing wait here, so rapid bursts
    // (e.g. several achievements unlocking at once) display one after another instead
    // of replacing each other mid-animation and overlapping on screen.
    private var pending: [Value] = []
    private var pendingDuration: TimeInterval? = nil

    /// When true, a `present` while content is showing enqueues the new value to appear
    /// after the current banner finishes. When false (the default, matching the original
    /// behavior), a new value replaces the visible banner immediately. Achievement toasts
    /// queue so a burst of unlocks plays back one at a time instead of overlapping.
    let queuesConcurrentPresentations: Bool

    init(defaultDisplayDuration: TimeInterval, queuesConcurrentPresentations: Bool = false) {
        self.defaultDisplayDuration = defaultDisplayDuration
        self.queuesConcurrentPresentations = queuesConcurrentPresentations
    }

    /// Presents `value`. Depending on `queuesConcurrentPresentations`, a value arriving
    /// while a banner is visible either replaces it immediately or waits in the queue.
    /// Show/hide use the same easeOut/easeIn 0.18s curves as the original toasts.
    func present(_ value: Value, displayDuration: TimeInterval? = nil) {
        if content != nil {
            if queuesConcurrentPresentations {
                if value != content {
                    pending.append(value)
                    pendingDuration = displayDuration
                }
                return
            }
            // Replace immediately: a newer status message should never wait behind a stale one.
            dismissTask?.cancel()
        }
        show(value, displayDuration: displayDuration)
    }

    private func show(_ value: Value, displayDuration: TimeInterval?) {
        generation += 1
        let currentGeneration = generation
        let duration = displayDuration ?? defaultDisplayDuration
        withAnimation(.easeOut(duration: 0.18)) {
            content = value
        }
        dismissTask = Task { @MainActor in
            try? await Task.sleep(for: .seconds(duration))
            guard !Task.isCancelled else { return }
            guard self.generation == currentGeneration else { return }
            withAnimation(.easeIn(duration: 0.18)) {
                self.content = nil
            }
            // Small gap so the outgoing banner fully fades before the next appears.
            if !self.pending.isEmpty {
                try? await Task.sleep(for: .milliseconds(160))
                guard !Task.isCancelled else { return }
                let next = self.pending.removeFirst()
                let nextDuration = self.pendingDuration
                self.show(next, displayDuration: nextDuration)
            }
        }
    }

    /// Cancels any pending auto-dismiss without changing the visible content.
    func cancelDismiss() {
        dismissTask?.cancel()
    }

    /// Drops the queue and any in-flight presentation. Used when the host view wants
    /// to fully reset transient state (e.g. the overlay is being torn down).
    func clear() {
        dismissTask?.cancel()
        pending.removeAll()
        pendingDuration = nil
        withAnimation(.easeIn(duration: 0.18)) {
            content = nil
        }
    }
}
