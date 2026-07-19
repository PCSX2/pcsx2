// BackgroundValidation.swift — Debug-only assertions for background model logic
// SPDX-License-Identifier: GPL-3.0+

import Foundation

#if DEBUG
enum BackgroundValidation {
    static func run() {
        testKindInference()
        testAssetCoding()
        testFitModeRoundTrip()
        testDynamicAppearanceCoding()
    }

    private static func testKindInference() {
        assert(BackgroundStorage.assetKind(for: URL(fileURLWithPath: "test.png")) == .image)
        assert(BackgroundStorage.assetKind(for: URL(fileURLWithPath: "test.gif")) == .animatedImage)
        assert(BackgroundStorage.assetKind(for: URL(fileURLWithPath: "test.mp4")) == .video)
    }

    private static func testAssetCoding() {
        let asset = BackgroundAsset(kind: .video, filename: "abc.mp4")
        let data = try! JSONEncoder().encode(asset)
        let decoded = try! JSONDecoder().decode(BackgroundAsset.self, from: data)
        assert(asset == decoded)
    }

    private static func testFitModeRoundTrip() {
        for mode in BackgroundFitMode.allCases {
            assert(BackgroundFitMode(rawValue: mode.rawValue) == mode)
        }
    }

    private static func testDynamicAppearanceCoding() {
        for style in DynamicBackgroundStyle.allCases {
            var preferences = DynamicAppearancePreferences.standard
            preferences.dynamicBackground = style

            let data = DynamicBackgroundCoding.encode(preferences)
            let decoded = data.flatMap {
                DynamicBackgroundCoding.decode(DynamicAppearancePreferences.self, from: $0)
            }

            assert(decoded == preferences)
        }
    }
}
#endif
