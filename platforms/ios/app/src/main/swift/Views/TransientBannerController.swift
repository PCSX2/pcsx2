// TransientBannerController.swift — auto-dismissing banner state machine
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

@MainActor
final class TransientBannerController<Value: Equatable>: ObservableObject {
    @Published var content: Value? = nil
    @Published private(set) var generation: Int = 0
    private var dismissTask: Task<Void, Never>? = nil
    private let defaultDisplayDuration: TimeInterval

    init(defaultDisplayDuration: TimeInterval) {
        self.defaultDisplayDuration = defaultDisplayDuration
    }

    /// Presents `value`, canceling any in-flight auto-dismiss. After `displayDuration`
    /// (defaulting to the controller's configured duration) the content is cleared.
    /// Show/hide use the same easeOut/easeIn 0.18s curves as the original toasts.
    func present(_ value: Value, displayDuration: TimeInterval? = nil) {
        dismissTask?.cancel()
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
        }
    }

    /// Cancels any pending auto-dismiss without changing the visible content.
    func cancelDismiss() {
        dismissTask?.cancel()
    }
}
