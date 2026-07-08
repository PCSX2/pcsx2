// BootSplashView.swift - Fullscreen boot intro video
// SPDX-License-Identifier: GPL-3.0+

import AVFoundation
import SwiftUI
import UIKit

struct BootSplashView: View {
    private static let hardTimeout: UInt64 = 6_000_000_000

    let onFinished: () -> Void
    @State private var finished = false

    var body: some View {
        ZStack {
            Color.black
                .ignoresSafeArea()

            BootSplashPlayerView(onFinished: finish)
                .ignoresSafeArea()
        }
        .contentShape(Rectangle())
        .onTapGesture {
            finish()
        }
        .task {
            try? await Task.sleep(nanoseconds: Self.hardTimeout)
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
        context.coordinator.player = player
        context.coordinator.observe(item: item)
        view.playerLayer.player = player
        player.play()

        return view
    }

    func updateUIView(_ uiView: BootSplashPlayerUIView, context: Context) {
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

        init(onFinished: @escaping () -> Void) {
            self.onFinished = onFinished
        }

        deinit {
            stopObserving()
        }

        func observe(item: AVPlayerItem) {
            stopObserving()

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

        func stopObserving() {
            if let endToken {
                NotificationCenter.default.removeObserver(endToken)
                self.endToken = nil
            }
            if let errorToken {
                NotificationCenter.default.removeObserver(errorToken)
                self.errorToken = nil
            }
            player = nil
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
