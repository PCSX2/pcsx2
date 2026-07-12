// BackgroundContainerView.swift — Orientation-aware background container
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import AVKit

struct BackgroundContainerView: View {
    @State private var settings = SettingsStore.shared
    let size: CGSize
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @Environment(\.accessibilityReduceTransparency) private var reduceTransparency
    @Environment(\.colorSchemeContrast) private var colorSchemeContrast
    @State private var videoPoster: UIImage?

    private var activeAsset: BackgroundAsset? {
        if size.width > size.height, let landscape = settings.backgroundLandscapeAsset { return landscape }
        return settings.backgroundPrimaryAsset
    }

    private var effectiveFitMode: BackgroundFitMode {
        size.width > size.height ? settings.backgroundLandscapeFitMode : settings.backgroundFitMode
    }

    private var effectiveDim: Double {
        let dim = settings.backgroundDim
        if reduceTransparency || colorSchemeContrast == .increased { return max(dim, 0.55) }
        return dim
    }

    var body: some View {
        ZStack {
            if let asset = activeAsset {
                let url = BackgroundStorage.fileURL(for: asset)
                switch asset.kind {
                case .image:
                    backgroundImage(UIImage(contentsOfFile: url.path))
                case .animatedImage:
                    if reduceMotion {
                        backgroundImage(UIImage(contentsOfFile: url.path))
                    } else {
                        animated(url)
                    }
                case .video:
                    if reduceMotion {
                        backgroundImage(videoPoster)
                            .task(id: url.path) { await generateVideoPoster(from: url) }
                    } else {
                        video(url)
                    }
                }
            }
            Color.black.opacity(effectiveDim)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .ignoresSafeArea()
    }

    private func animated(_ url: URL) -> some View {
        AnimatedLibraryBackgroundView(url: url, fitMode: effectiveFitMode)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .clipped()
    }

    private func video(_ url: URL) -> some View {
        VideoBackgroundView(url: url, muted: settings.backgroundVideoMuted, fitMode: effectiveFitMode)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .clipped()
    }

    @ViewBuilder
    private func backgroundImage(_ image: UIImage?) -> some View {
        if let image {
            Image(uiImage: image)
                .resizable()
                .applyBackgroundFitMode(effectiveFitMode, size: size)
        }
    }

    private func generateVideoPoster(from url: URL) async {
        let asset = AVAsset(url: url)
        let generator = AVAssetImageGenerator(asset: asset)
        generator.appliesPreferredTrackTransform = true
        do {
            let (cgImage, _) = try await generator.image(at: .zero)
            videoPoster = UIImage(cgImage: cgImage)
        } catch {
            videoPoster = nil
        }
    }
}

private extension Image {
    @ViewBuilder
    func applyBackgroundFitMode(_ mode: BackgroundFitMode, size: CGSize) -> some View {
        switch mode {
        case .fill:
            self.scaledToFill().frame(width: size.width, height: size.height).clipped()
        case .fit:
            self.scaledToFit().frame(width: size.width, height: size.height)
        case .stretch:
            self.frame(width: size.width, height: size.height).clipped()
        }
    }
}
