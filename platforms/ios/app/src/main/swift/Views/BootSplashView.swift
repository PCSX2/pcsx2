// BootSplashView.swift - Fullscreen boot intro video
// SPDX-License-Identifier: GPL-3.0+

import AVFoundation
import SwiftUI
import UIKit

struct BootSplashView: View {
    private static let hardTimeout: UInt64 = 6_000_000_000
    private static let idleVMPrewarmResolved = Notification.Name(
        "ARMSX2iOSIdleVMPrewarmResolved"
    )

    let onFinished: () -> Void
    @State private var finished = false
    @State private var playbackReady = ARMSX2Bridge.isIdleVMPrewarmResolved()

    var body: some View {
        ZStack {
            Color.black
                .ignoresSafeArea()

            BootSplashPlayerView(shouldPlay: playbackReady, onFinished: finish)
                .ignoresSafeArea()
        }
        .contentShape(Rectangle())
        .onTapGesture {
            finish()
        }
        .onAppear {
            if ARMSX2Bridge.isIdleVMPrewarmResolved() {
                playbackReady = true
            }
        }
        .onReceive(NotificationCenter.default.publisher(for: Self.idleVMPrewarmResolved)) { _ in
            playbackReady = true
        }
        .task(id: playbackReady) {
            guard playbackReady else {
                return
            }
            try? await Task.sleep(nanoseconds: Self.hardTimeout)
            guard !Task.isCancelled else {
                return
            }
            finish()
        }
    }

    @MainActor
    private func finish() {
        guard !finished else {
            return
        }

        finished = true
        onFinished()
    }
}

private struct BootSplashPlayerView: UIViewRepresentable {
    let shouldPlay: Bool
    let onFinished: () -> Void

    func makeCoordinator() -> Coordinator {
        Coordinator(onFinished: onFinished)
    }

    func makeUIView(context: Context) -> BootSplashPlayerUIView {
        let view = BootSplashPlayerUIView()
        view.backgroundColor = .black
        view.playerLayer.videoGravity = .resizeAspect

        guard let url = Bundle.main.url(forResource: "boot_intro", withExtension: "mp4") else {
            DispatchQueue.main.async {
                context.coordinator.finish()
            }
            return view
        }

        let item = AVPlayerItem(url: url)
        let player = AVPlayer(playerItem: item)
        player.actionAtItemEnd = .pause
        context.coordinator.configure(player: player, item: item)
        view.playerLayer.player = player
        if shouldPlay {
            context.coordinator.startIfNeeded()
        }

        return view
    }

    func updateUIView(_ uiView: BootSplashPlayerUIView, context: Context) {
        if shouldPlay {
            context.coordinator.startIfNeeded()
        }
    }

    static func dismantleUIView(_ uiView: BootSplashPlayerUIView, coordinator: Coordinator) {
        uiView.playerLayer.player?.pause()
        uiView.playerLayer.player = nil
        coordinator.stopObserving()
    }

    final class Coordinator: @unchecked Sendable {
        var player: AVPlayer?
        private let onFinished: () -> Void
        private var endToken: NSObjectProtocol?
        private var errorToken: NSObjectProtocol?
        private var hasStarted = false

        init(onFinished: @escaping () -> Void) {
            self.onFinished = onFinished
        }

        deinit {
            stopObserving()
        }

        func configure(player: AVPlayer, item: AVPlayerItem) {
            stopObserving()
            self.player = player

            endToken = NotificationCenter.default.addObserver(
                forName: .AVPlayerItemDidPlayToEndTime,
                object: item,
                queue: .main
            ) { [weak self] _ in
                self?.finish()
            }

            errorToken = NotificationCenter.default.addObserver(
                forName: .AVPlayerItemFailedToPlayToEndTime,
                object: item,
                queue: .main
            ) { [weak self] _ in
                self?.finish()
            }
        }

        func startIfNeeded() {
            guard !hasStarted, let player else {
                return
            }

            hasStarted = true
            player.seek(to: .zero)
            player.play()
        }

        func stopObserving() {
            if let endToken {
                NotificationCenter.default.removeObserver(endToken)
                self.endToken = nil
            }
            if let errorToken {
                NotificationCenter.default.removeObserver(errorToken)
                self.errorToken = nil
            }
            player?.pause()
            player = nil
            hasStarted = false
        }

        func finish() {
            onFinished()
        }
    }
}

private final class BootSplashPlayerUIView: UIView {
    override static var layerClass: AnyClass {
        AVPlayerLayer.self
    }

    var playerLayer: AVPlayerLayer {
        layer as! AVPlayerLayer
    }
}
