// BackgroundAsset.swift — Library background model types
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

enum BackgroundAssetKind: String, Codable, CaseIterable, Identifiable, Equatable, Sendable {
    case image
    case animatedImage
    case video
    var id: String { rawValue }
}

enum BackgroundFitMode: String, Codable, CaseIterable, Identifiable, Equatable, Sendable {
    case fill, fit, stretch
    var id: String { rawValue }
}

struct BackgroundAsset: Codable, Identifiable, Equatable, Sendable {
    let id: UUID
    let kind: BackgroundAssetKind
    let filename: String

    init(id: UUID = UUID(), kind: BackgroundAssetKind, filename: String) {
        self.id = id
        self.kind = kind
        self.filename = filename
    }
}
