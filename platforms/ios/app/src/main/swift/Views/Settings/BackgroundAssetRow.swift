// BackgroundAssetRow.swift — Reusable background asset settings row
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct BackgroundAssetRow: View {
    let title: String
    let asset: BackgroundAsset?
    let glyph: String
    let caption: String?
    let action: () -> Void

    private var displayedCaption: String {
        if let asset { return asset.filename }
        return caption ?? ""
    }

    var body: some View {
        Button(action: action) {
            HStack(spacing: 12) {
                ZStack {
                    thumbnail
                    if asset != nil { kindBadge }
                }
                .frame(width: 64, height: 40)
                .clipped()
                .cornerRadius(6)
                .overlay(RoundedRectangle(cornerRadius: 6).stroke(Color.secondary.opacity(0.2), lineWidth: 1))

                VStack(alignment: .leading, spacing: 2) {
                    HStack(spacing: 6) {
                        Image(systemName: glyph).font(.caption).foregroundStyle(.secondary)
                        Text(title).font(.body).foregroundStyle(.primary)
                    }
                    if asset == nil, !displayedCaption.isEmpty {
                        Text(displayedCaption).font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                    }
                    if let asset {
                        Text(metaLine(for: asset)).font(.caption2).foregroundStyle(.secondary).lineLimit(1)
                    }
                }
                .multilineTextAlignment(.leading)

                Spacer()
                Image(systemName: "chevron.right").foregroundStyle(.secondary).font(.caption)
            }
            .frame(minHeight: 56, alignment: .leading)
        }
    }

    @ViewBuilder
    private var thumbnail: some View {
        if let asset {
            let url = BackgroundStorage.fileURL(for: asset)
            switch asset.kind {
            case .image, .animatedImage:
                if let image = UIImage(contentsOfFile: url.path) {
                    Image(uiImage: image).resizable().aspectRatio(contentMode: .fill)
                } else { placeholder }
            case .video:
                ZStack {
                    Color.secondary.opacity(0.15)
                    Image(systemName: "film").resizable().aspectRatio(contentMode: .fit).padding(10).foregroundStyle(.secondary)
                }
            }
        } else { placeholder }
    }

    private var placeholder: some View {
        ZStack {
            Color.secondary.opacity(0.15)
            Image(systemName: glyph).foregroundStyle(.secondary)
        }
    }

    private var kindBadge: some View {
        VStack {
            HStack {
                Spacer()
                Image(systemName: kindGlyph).font(.system(size: 9)).foregroundStyle(.white)
                    .padding(3)
                    .background(.black.opacity(0.45), in: Circle())
                    .padding(3)
            }
            Spacer()
        }
    }

    private var kindGlyph: String {
        guard let asset else { return "" }
        switch asset.kind {
        case .image: return "photo"
        case .animatedImage: return "play.rectangle"
        case .video: return "film"
        }
    }

    private func metaLine(for asset: BackgroundAsset) -> String {
        var parts: [String] = []
        switch asset.kind {
        case .image: parts.append(NSLocalizedString("Image", comment: ""))
        case .animatedImage: parts.append(NSLocalizedString("Animated", comment: ""))
        case .video: parts.append(NSLocalizedString("Video", comment: ""))
        }
        if let bytes = BackgroundStorage.fileSize(of: asset) {
            parts.append(ByteCountFormatter.string(fromByteCount: bytes, countStyle: .file))
        }
        return parts.joined(separator: " · ")
    }
}
