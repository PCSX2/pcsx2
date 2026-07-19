// VideoBackgroundView.swift — Looping video background renderer
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import AVKit

struct VideoBackgroundView: View {
    let url: URL
    let muted: Bool
    let fitMode: BackgroundFitMode

    var body: some View { VideoBackgroundPlayer(url: url, muted: muted, fitMode: fitMode) }
}

private struct VideoBackgroundPlayer: UIViewRepresentable {
    let url: URL
    let muted: Bool
    let fitMode: BackgroundFitMode

    func makeUIView(context: Context) -> LoopingVideoView { LoopingVideoView() }
    func updateUIView(_ uiView: LoopingVideoView, context: Context) { uiView.configure(url: url, muted: muted, fitMode: fitMode) }

	static func dismantleUIView(_ uiView: LoopingVideoView, coordinator: ()) {
		MainActor.assumeIsolated { uiView.teardown() }
	}
}

private final class LoopingVideoView: UIView {
    private var playerLayer: AVPlayerLayer?
    private var playerLooper: AVPlayerLooper?
    private var player: AVQueuePlayer?
    private var currentURL: URL?
    private var releasedForGameplay = false

    deinit {
        MainActor.assumeIsolated {
			teardown()
        }
    }

    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .clear
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
        backgroundColor = .clear
    }

    func configure(url: URL, muted: Bool, fitMode: BackgroundFitMode) {
        guard !releasedForGameplay else { return }
        let gravity = videoGravity(for: fitMode)

        // Rebuild the player when the source changes so swapping the background
        // (portrait ↔ landscape, or replacing a video) actually takes effect.
        // The early-return-on-existing-player meant a second video was silently ignored.
        if player != nil, currentURL != url {
			teardown()
        }

        if player != nil {
            player?.isMuted = muted
            playerLayer?.videoGravity = gravity
            return
        }

        currentURL = url
        let item = AVPlayerItem(url: url)
        let queuePlayer = AVQueuePlayer(playerItem: item)
        queuePlayer.isMuted = muted
        queuePlayer.allowsExternalPlayback = false
        let looper = AVPlayerLooper(player: queuePlayer, templateItem: item)
        let layer = AVPlayerLayer(player: queuePlayer)
        layer.videoGravity = gravity
        layer.backgroundColor = UIColor.clear.cgColor
        self.playerLayer?.removeFromSuperlayer()
        self.layer.addSublayer(layer)
        self.playerLayer = layer
        self.playerLooper = looper
        self.player = queuePlayer
        layoutIfNeeded()

        NotificationCenter.default.addObserver(self, selector: #selector(pause), name: UIApplication.didEnterBackgroundNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(pause), name: UIScene.willDeactivateNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(resumeIfReady), name: UIScene.didActivateNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(releaseResourcesForGameplay), name: AppState.releaseMenuBackgroundResourcesNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(adaptToPowerState), name: .NSProcessInfoPowerStateDidChange, object: nil)
        adaptToPowerState()
    }

	func teardown() {
        player?.pause()
        NotificationCenter.default.removeObserver(self)
        playerLayer?.removeFromSuperlayer()
        player = nil
        playerLooper = nil
        playerLayer = nil
		currentURL = nil
    }

    @objc private func pause() { player?.pause() }

    @objc private func resumeIfReady() {
        guard UIApplication.shared.applicationState != .background, !ProcessInfo.processInfo.isLowPowerModeEnabled else { return }
        player?.seek(to: .zero)
        player?.play()
    }

    @objc private func releaseResourcesForGameplay() {
        releasedForGameplay = true
        teardown()
    }

    @objc private func adaptToPowerState() {
        if ProcessInfo.processInfo.isLowPowerModeEnabled {
            pause()
        } else {
            resumeIfReady()
        }
    }

    private func videoGravity(for fitMode: BackgroundFitMode) -> AVLayerVideoGravity {
        switch fitMode {
        case .fill: return .resizeAspectFill
        case .fit: return .resizeAspect
        case .stretch: return .resize
        }
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        guard let playerLayer else { return }
        let resolved = bounds.isEmpty ? superview?.bounds ?? bounds : bounds
        playerLayer.frame = resolved
        playerLayer.setNeedsDisplay()
        playerLayer.displayIfNeeded()
    }
}

private extension CGRect {
    var isEmpty: Bool { width <= 0 || height <= 0 }
}
