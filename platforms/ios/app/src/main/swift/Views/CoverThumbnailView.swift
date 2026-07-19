// CoverThumbnailView.swift - Cover thumbnail rendering and cache
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit
import ImageIO

struct CoverThumbnailView: View {
    let gameName: String
    let coverURL: URL?
    let coverSignature: String?
    let width: CGFloat
    let height: CGFloat

    @State private var image: UIImage?

    private var cacheID: String {
        "\(coverSignature ?? "placeholder")|\(Int(width))x\(Int(height))"
    }

    var body: some View {
        ZStack {
            RoundedRectangle(cornerRadius: 10, style: .continuous)
                .fill(Color(.secondarySystemGroupedBackground))
            RoundedRectangle(cornerRadius: 10, style: .continuous)
                .strokeBorder(.white.opacity(0.12), lineWidth: 1)

            if let image {
                Image(uiImage: image)
                    .resizable()
                    .scaledToFill()
                    .frame(width: width, height: height)
                    .clipped()
            } else {
                VStack(spacing: 6) {
                    Image(systemName: gameName.lowercased().hasSuffix(".chd") ? "archivebox" : "opticaldisc")
                        .font(.system(size: 24, weight: .medium))
                    Text(gameName.lowercased().hasSuffix(".chd") ? "CHD" : "PS2")
                        .font(.caption2)
                        .fontWeight(.bold)
                }
                .foregroundStyle(.secondary)
            }
        }
        .frame(width: width, height: height)
        .clipShape(RoundedRectangle(cornerRadius: 10, style: .continuous))
        .task(id: cacheID) {
            await loadThumbnail()
        }
        .accessibilityHidden(true)
    }

    @MainActor
    private func loadThumbnail() async {
        guard let coverURL else {
            image = nil
            return
        }

        let scale = UIScreen.main.scale
        if let cached = CoverThumbnailCache.shared.cachedImage(for: coverURL, signature: coverSignature, width: width, height: height, scale: scale) {
            image = cached
            return
        }

        let loadedImage = await CoverThumbnailCache.shared.thumbnail(
            for: coverURL,
            signature: coverSignature,
            width: width,
            height: height,
            scale: scale
        )
        guard !Task.isCancelled else { return }
        image = loadedImage
    }
}

final class CoverThumbnailCache: @unchecked Sendable {
    static let shared = CoverThumbnailCache()

    private let cache = NSCache<NSString, UIImage>()
    private let stateLock = NSLock()
    private var generation: UInt64 = 0
    private var acceptsImages = true

    private init() {
        cache.countLimit = 768
        cache.totalCostLimit = 96 * 1024 * 1024
    }

    func cachedImage(for url: URL, signature: String?, width: CGFloat, height: CGFloat, scale: CGFloat) -> UIImage? {
        stateLock.lock()
        defer { stateLock.unlock() }
        guard acceptsImages else { return nil }
        return cache.object(forKey: cacheKey(for: url, signature: signature, width: width, height: height, scale: scale))
    }

    func thumbnail(for url: URL, signature: String?, width: CGFloat, height: CGFloat, scale: CGFloat) async -> UIImage? {
        let key = cacheKey(for: url, signature: signature, width: width, height: height, scale: scale)
        guard let request = beginRequest(for: key) else { return nil }
        if let cachedImage = request.cachedImage {
            return cachedImage
        }
        let requestGeneration = request.generation

        let path = url.path
        let maxPixelSize = max(1, Int(max(width, height) * scale))
        let decodeTask = Task.detached(priority: .utility) {
            guard !Task.isCancelled else { return nil as UIImage? }
            let sourceOptions = [kCGImageSourceShouldCache: false] as CFDictionary
            let thumbnailOptions = [
                kCGImageSourceCreateThumbnailFromImageAlways: true,
                kCGImageSourceCreateThumbnailWithTransform: true,
                kCGImageSourceShouldCacheImmediately: true,
                kCGImageSourceThumbnailMaxPixelSize: maxPixelSize,
            ] as CFDictionary

            guard let source = CGImageSourceCreateWithURL(url as CFURL, sourceOptions),
                  let cgImage = CGImageSourceCreateThumbnailAtIndex(source, 0, thumbnailOptions) else {
                let fallback = UIImage(contentsOfFile: path)
                return Task.isCancelled ? nil : fallback
            }

            guard !Task.isCancelled else { return nil }
            return UIImage(cgImage: cgImage, scale: scale, orientation: .up)
        }
        let image = await withTaskCancellationHandler {
            await decodeTask.value
        } onCancel: {
            decodeTask.cancel()
        }

        guard !Task.isCancelled, let image else { return nil }
        let cost = max(1, Int(image.size.width * image.scale * image.size.height * image.scale * 4))
        guard insert(image, for: key, cost: cost, generation: requestGeneration) else { return nil }

        return image
    }

    static func signature(for url: URL?) -> String? {
        guard let url else { return nil }
        let values = try? url.resourceValues(forKeys: [.contentModificationDateKey, .fileSizeKey])
        let modified = values?.contentModificationDate?.timeIntervalSince1970 ?? 0
        let size = values?.fileSize ?? 0
        return "\(url.path)|\(modified)|\(size)"
    }

    func activateForMenu() {
        stateLock.lock()
        defer { stateLock.unlock() }
        guard !acceptsImages else { return }
        generation &+= 1
        acceptsImages = true
    }

    func releaseForGameplay() {
        stateLock.lock()
        generation &+= 1
        acceptsImages = false
        cache.removeAllObjects()
        stateLock.unlock()
    }

    private func beginRequest(for key: NSString) -> (generation: UInt64, cachedImage: UIImage?)? {
        stateLock.lock()
        defer { stateLock.unlock() }
        guard acceptsImages else { return nil }
        return (generation, cache.object(forKey: key))
    }

    private func insert(_ image: UIImage, for key: NSString, cost: Int, generation requestGeneration: UInt64) -> Bool {
        stateLock.lock()
        defer { stateLock.unlock() }
        guard acceptsImages, generation == requestGeneration else { return false }
        cache.setObject(image, forKey: key, cost: cost)
        return true
    }

    private func cacheKey(for url: URL, signature: String?, width: CGFloat, height: CGFloat, scale: CGFloat) -> NSString {
        let pixelWidth = Int(width * scale)
        let pixelHeight = Int(height * scale)
        return "\(signature ?? url.path)|\(pixelWidth)x\(pixelHeight)" as NSString
    }
}
