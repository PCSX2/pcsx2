// AnimatedLibraryBackgroundView.swift — animated library wallpaper renderer
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import ImageIO
import UIKit

/// Decodes and animates a multi-frame image (GIF, APNG, animated WebP when
/// ImageIO supports it) for the library background. Frames are decoded lazily
/// one at a time on a timer to avoid the full-frame memory blowup of
/// `UIImage.animatedImage`. Falls back to a static first frame when Reduce
/// Motion is on, when the file is not animated, or when it exceeds the safety
/// caps enforced by `AnimatedBackgroundLoader`.
struct AnimatedLibraryBackgroundView: View {
    let url: URL
    let fitMode: BackgroundFitMode
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @State private var frames: [AnimatedBackgroundLoader.Frame] = []
    @State private var loadFailed = false

    var body: some View {
        Group {
            if reduceMotion || frames.isEmpty {
                staticFirstFrame
            } else {
                AnimatedFramePlayer(frames: frames, fitMode: fitMode)
            }
        }
        .task(id: url.path) {
            guard frames.isEmpty, !loadFailed else { return }
            // Decode off the MainActor so a large multi-frame image cannot
            // stall the UI while the library is presented.
			let loader = Task.detached(priority: .utility) {
                AnimatedBackgroundLoader.loadFrames(from: url)
			}
			let loaded = await withTaskCancellationHandler {
				await loader.value
			} onCancel: {
				loader.cancel()
			}
            // `.task(id:)` cancels this task when url changes; drop the result.
            guard !Task.isCancelled else { return }
            if loaded.isEmpty {
                loadFailed = true
            } else {
                frames = loaded
            }
        }
    }

    /// Single first-frame fallback used under Reduce Motion or when frame
    /// decoding is unavailable. Keeps the library usable even if the animated
    /// payload can't be played.
    private var staticFirstFrame: some View {
        GeometryReader { geometry in
            if let image = AnimatedBackgroundLoader.staticImage(from: url) {
                Image(uiImage: image)
                    .resizable()
                    .applyBackgroundFitMode(fitMode)
                    .frame(width: geometry.size.width, height: geometry.size.height)
                    .clipped()
            }
        }
    }
}

/// Drives frame playback with a `UIImageView` so animation is handled by UIKit
/// without per-frame SwiftUI re-renders. Playback pauses automatically when the
/// app moves to the background (via `UIApplication` notifications) and the view
/// leaves the hierarchy.
private struct AnimatedFramePlayer: UIViewRepresentable {
    let frames: [AnimatedBackgroundLoader.Frame]
    let fitMode: BackgroundFitMode

    func makeUIView(context: Context) -> AnimatedBackgroundImageView {
        AnimatedBackgroundImageView()
    }

    func updateUIView(_ uiView: AnimatedBackgroundImageView, context: Context) {
        uiView.configure(with: frames, fitMode: fitMode)
    }

    static func dismantleUIView(_ uiView: AnimatedBackgroundImageView, coordinator: ()) {
        // UIKit calls this on the main thread; assume MainActor to call the
        // UIView's MainActor-isolated cleanup.
        MainActor.assumeIsolated { uiView.teardown() }
    }
}

/// UIImageView subclass that owns the playback timer and lifecycle observers.
private final class AnimatedBackgroundImageView: UIView {
    private let imageView = UIImageView()
    private var frames: [AnimatedBackgroundLoader.Frame] = []
    private var index = 0
    private var displayLink: CADisplayLink?
    private var accumulated: CFTimeInterval = 0
    private var lastTimestamp: CFTimeInterval = 0
    private var releasedForGameplay = false

    override init(frame: CGRect) {
        super.init(frame: frame)
        imageView.clipsToBounds = true
        addSubview(imageView)
        // Pause while backgrounded to avoid burning CPU/GPU off-screen.
        NotificationCenter.default.addObserver(
            self, selector: #selector(pause),
            name: UIApplication.didEnterBackgroundNotification, object: nil)
        NotificationCenter.default.addObserver(
            self, selector: #selector(pause),
            name: UIScene.willDeactivateNotification, object: nil)
        NotificationCenter.default.addObserver(
            self, selector: #selector(resumeIfReady),
            name: UIScene.didActivateNotification, object: nil)
        NotificationCenter.default.addObserver(
            self, selector: #selector(releaseResourcesForGameplay),
            name: AppState.releaseMenuBackgroundResourcesNotification, object: nil)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) { fatalError() }

    deinit {
        // UIKit tears down views on the main thread; assert it so the
        // MainActor-isolated stop() can be called.
        MainActor.assumeIsolated {
            teardown()
        }
    }

    func configure(with frames: [AnimatedBackgroundLoader.Frame], fitMode: BackgroundFitMode) {
        guard !releasedForGameplay else { return }
        imageView.contentMode = uiContentMode(for: fitMode)
        guard frames != self.frames, !frames.isEmpty else { return }
        stop()
        self.frames = frames
        index = 0
        imageView.image = frames.first?.image
        resumeIfReady()
    }

    func stop() {
        displayLink?.invalidate()
        displayLink = nil
        accumulated = 0
        lastTimestamp = 0
    }

    func teardown() {
        stop()
        frames.removeAll(keepingCapacity: false)
        imageView.image = nil
        NotificationCenter.default.removeObserver(self)
    }

    @objc private func pause() {
        displayLink?.invalidate()
        displayLink = nil
        lastTimestamp = 0
    }

    @objc private func resumeIfReady() {
        guard !releasedForGameplay, displayLink == nil, !frames.isEmpty,
              UIApplication.shared.applicationState != .background else { return }
        let link = CADisplayLink(target: self, selector: #selector(tick))
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    @objc private func releaseResourcesForGameplay() {
        releasedForGameplay = true
        teardown()
    }

    @objc private func tick(link: CADisplayLink) {
        if lastTimestamp == 0 {
            lastTimestamp = link.timestamp
            return
        }
        let delta = link.timestamp - lastTimestamp
        lastTimestamp = link.timestamp
        accumulated += delta
        let duration = frames[index].duration
        guard duration > 0, accumulated >= duration else { return }
        accumulated -= duration
        index = (index + 1) % frames.count
        imageView.image = frames[index].image
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        imageView.frame = bounds
    }

    private func uiContentMode(for fitMode: BackgroundFitMode) -> UIView.ContentMode {
        switch fitMode {
        case .fill: return .scaleAspectFill
        case .fit: return .scaleAspectFit
        case .stretch: return .scaleToFill
        }
    }
}

/// Loads and validates animated image frames with safety caps. A file that
/// would decode to too many frames or too much memory is treated as static
/// (callers fall back to the first frame).
enum AnimatedBackgroundLoader {
    /// Per-frame decoded image and its display duration in seconds.
    struct Frame: Equatable {
        let image: UIImage
        let duration: TimeInterval
    }

    /// Guard rails to keep animated backgrounds from consuming too much memory
    /// or CPU on handheld devices.
    static let maxFrames = 120
    static let maxFrameDimension: CGFloat = 1280

    /// True when the file at `url` is a multi-frame image that the loader will
    /// animate. Used to decide between the animated and static render paths.
    static func isAnimated(_ url: URL) -> Bool {
        guard let source = cgSource(url) else { return false }
        return CGImageSourceGetCount(source) > 1
    }

    /// Decodes the frames of an animated image, honoring per-frame durations
    /// and the safety caps. Returns an empty array on failure or when caps are
    /// exceeded (caller falls back to a static image).
    static func loadFrames(from url: URL) -> [Frame] {
        guard let source = cgSource(url) else { return [] }
        let count = CGImageSourceGetCount(source)
        guard count > 1, count <= maxFrames else { return [] }

        var frames: [Frame] = []
        frames.reserveCapacity(count)
        for i in 0..<count {
			guard !Task.isCancelled else { return [] }
            guard let cgImage = CGImageSourceCreateImageAtIndex(source, i, nil) else { continue }
            // Skip oversized frames to avoid memory spikes; treat as static.
            if CGFloat(cgImage.width) > maxFrameDimension || CGFloat(cgImage.height) > maxFrameDimension {
                return []
            }
            let image = UIImage(cgImage: cgImage)
            let duration = frameDuration(at: i, in: source)
            frames.append(Frame(image: image, duration: duration))
        }
        return frames.isEmpty || frames.count < 2 ? [] : frames
    }

    /// Single static image for the fallback path.
    static func staticImage(from url: URL) -> UIImage? {
        UIImage(contentsOfFile: url.path)
    }

    private static func cgSource(_ url: URL) -> CGImageSource? {
        CGImageSourceCreateWithURL(url as CFURL, nil)
    }

    /// ImageIO hides per-frame durations in properties; GIF uses
    /// `{GIF, DelayTime}`, APNG uses `{PNG, UnclampedDelayTime}` (falling back
    /// to `DelayTime`). Defaults to 0.1s when unspecified.
    private static func frameDuration(at index: Int, in source: CGImageSource) -> TimeInterval {
        let properties = CGImageSourceCopyPropertiesAtIndex(source, index, nil) as? [String: Any] ?? [:]
        if let gif = properties[kCGImagePropertyGIFDictionary as String] as? [String: Any],
           let delay = gif[kCGImagePropertyGIFDelayTime as String] as? Double, delay > 0 {
            return delay
        }
        if let png = properties[kCGImagePropertyPNGDictionary as String] as? [String: Any] {
            if let delay = png[kCGImagePropertyAPNGUnclampedDelayTime as String] as? Double {
                return delay
            }
            if let delay = png[kCGImagePropertyAPNGDelayTime as String] as? Double, delay > 0 {
                return delay
            }
        }
        return 0.1
    }
}

private extension Image {
    @ViewBuilder
    func applyBackgroundFitMode(_ mode: BackgroundFitMode) -> some View {
        switch mode {
        case .fill:
            self.scaledToFill()
        case .fit:
            self.scaledToFit()
        case .stretch:
            self
        }
    }
}
