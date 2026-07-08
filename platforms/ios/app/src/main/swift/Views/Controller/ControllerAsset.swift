// ControllerAsset.swift — controller skin/image asset resolution
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit

enum ControllerAsset {
    private static let edgeToEdgePortraitAspectRatio: CGFloat = 1.55
    private static let analogBaseCommonFileName = "ic_controller_analog_base.png"
    private static let analogBaseLeftFileName = "ic_controller_analog_base_left.png"
    private static let analogBaseRightFileName = "ic_controller_analog_base_right.png"
    private static let analogStickCurrentFileName = "ic_controller_analog_stick.png"
    private static let legacyAnalogStickFileName = "ic_controller_analog_button.png"
    private static let analogStickLeftFileName = "ic_controller_analog_stick_left.png"
    private static let analogStickRightFileName = "ic_controller_analog_stick_right.png"
    private static let legacyAnalogStickLeftFileName = "ic_controller_analog_button_left.png"
    private static let legacyAnalogStickRightFileName = "ic_controller_analog_button_right.png"

    static func fileName(for button: ARMSX2PadButton) -> String {
        switch button {
        case .up:       return "ic_controller_up_button.png"
        case .down:     return "ic_controller_down_button.png"
        case .left:     return "ic_controller_left_button.png"
        case .right:    return "ic_controller_right_button.png"
        case .cross:    return "ic_controller_cross_button.png"
        case .circle:   return "ic_controller_circle_button.png"
        case .square:   return "ic_controller_square_button.png"
        case .triangle: return "ic_controller_triangle_button.png"
        case .L1:       return "ic_controller_l1_button.png"
        case .R1:       return "ic_controller_r1_button.png"
        case .L2:       return "ic_controller_l2_button.png"
        case .R2:       return "ic_controller_r2_button.png"
        case .start:    return "ic_controller_start_button.png"
        case .select:   return "ic_controller_select_button.png"
        case .L3:       return "ic_controller_l3_button.png"
        case .R3:       return "ic_controller_r3_button.png"
        @unknown default:
            return ""
        }
    }

    static func analogBaseFileName(isLeft: Bool, exists: (String) -> Bool) -> String {
        let sideFileName = isLeft ? analogBaseLeftFileName : analogBaseRightFileName
        return exists(sideFileName) ? sideFileName : analogBaseCommonFileName
    }

    static func analogBaseFileName(
        isLeft: Bool,
        descriptor: VPadSkinDescriptor,
        skinLibrary: VPadSkinLibraryStore = .shared
    ) -> String {
        analogBaseFileName(isLeft: isLeft) { image(named: $0, descriptor: descriptor, skinLibrary: skinLibrary) != nil }
    }

    static func analogStickFileName(isLeft: Bool, exists: (String) -> Bool) -> String {
        let sideFileName = isLeft ? analogStickLeftFileName : analogStickRightFileName
        let legacySideFileName = isLeft ? legacyAnalogStickLeftFileName : legacyAnalogStickRightFileName
        if exists(sideFileName) {
            return sideFileName
        }
        if exists(legacySideFileName) {
            return legacySideFileName
        }
        if exists(analogStickCurrentFileName) {
            return analogStickCurrentFileName
        }
        if exists(legacyAnalogStickFileName) {
            return legacyAnalogStickFileName
        }
        return analogStickCurrentFileName
    }

    static func analogStickFileName(
        isLeft: Bool,
        descriptor: VPadSkinDescriptor,
        skinLibrary: VPadSkinLibraryStore = .shared
    ) -> String {
        analogStickFileName(isLeft: isLeft) { skinContainsExactAsset(named: $0, descriptor: descriptor, skinLibrary: skinLibrary) }
    }

    static func image(named fileName: String, skin: VirtualPadSkin) -> UIImage? {
        guard !fileName.isEmpty else { return nil }

        let baseName = (fileName as NSString).deletingPathExtension
        if skin == .custom,
           let directory = VirtualPadSkin.customSkinDirectory(),
           let customImage = customImage(named: fileName, baseName: baseName, directory: directory) {
            return customImage
        }

        if let directoryName = skin.bundledDirectoryName,
           let bundledImage = bundledSkinImage(named: baseName, directoryName: directoryName) {
            return bundledImage
        }

        if let image = UIImage(named: baseName) ?? UIImage(named: fileName) {
            return image
        }

        guard let path = Bundle.main.path(forResource: baseName, ofType: "png") else {
            return nil
        }

        return UIImage(contentsOfFile: path)
    }

    static func image(
        named fileName: String,
        descriptor: VPadSkinDescriptor,
        skinLibrary: VPadSkinLibraryStore = .shared
    ) -> UIImage? {
        guard !fileName.isEmpty else { return nil }

        let skin = descriptor.virtualPadSkin
        let baseName = (fileName as NSString).deletingPathExtension
        if descriptor.source == .imported,
           let directory = skinLibrary.importedAssetsDirectory(for: descriptor),
           let customImage = customImage(named: fileName, baseName: baseName, directory: directory) {
            return customImage
        }

        if isLegacyCustomDescriptor(descriptor),
           let directory = VirtualPadSkin.legacyCustomSkinDirectory(),
           let customImage = customImage(named: fileName, baseName: baseName, directory: directory) {
            return customImage
        }

        if let directoryName = skin.bundledDirectoryName,
           let bundledImage = bundledSkinImage(named: baseName, directoryName: directoryName) {
            return bundledImage
        }

        if let image = UIImage(named: baseName) ?? UIImage(named: fileName) {
            return image
        }

        guard let path = Bundle.main.path(forResource: baseName, ofType: "png") else {
            return nil
        }

        return UIImage(contentsOfFile: path)
    }

    private static func skinContainsExactAsset(
        named fileName: String,
        descriptor: VPadSkinDescriptor,
        skinLibrary: VPadSkinLibraryStore
    ) -> Bool {
        let baseName = (fileName as NSString).deletingPathExtension
        let skin = descriptor.virtualPadSkin
        if descriptor.source == .imported,
           let directory = skinLibrary.importedAssetsDirectory(for: descriptor) {
            return FileManager.default.fileExists(atPath: directory.appendingPathComponent(fileName).path)
        }

        if isLegacyCustomDescriptor(descriptor),
           let directory = VirtualPadSkin.legacyCustomSkinDirectory() {
            return FileManager.default.fileExists(atPath: directory.appendingPathComponent(fileName).path)
        }

        if let directoryName = skin.bundledDirectoryName {
            return Bundle.main.url(forResource: baseName, withExtension: "png", subdirectory: "controller_skins/\(directoryName)") != nil
        }

        return UIImage(named: baseName) != nil || UIImage(named: fileName) != nil || Bundle.main.path(forResource: baseName, ofType: "png") != nil
    }

    private static func bundledSkinImage(named baseName: String, directoryName: String) -> UIImage? {
        let subdirectory = "controller_skins/\(directoryName)"
        guard let url = Bundle.main.url(forResource: baseName, withExtension: "png", subdirectory: subdirectory) else {
            return nil
        }

        return UIImage(contentsOfFile: url.path)
    }

    static func fullSkinImage(
        descriptor: VPadSkinDescriptor,
        isLandscape: Bool,
        skinLibrary: VPadSkinLibraryStore = .shared
    ) -> UIImage? {
        let skin = descriptor.virtualPadSkin
        let directory: URL?
        if descriptor.source == .imported {
            directory = skinLibrary.importedAssetsDirectory(for: descriptor)
        } else if isLegacyCustomDescriptor(descriptor) {
            directory = VirtualPadSkin.legacyCustomSkinDirectory()
        } else {
            directory = nil
        }
        guard let directory else {
            return nil
        }

        let orientationCandidates = isLandscape
            ? ["controller_edgetoedge_landscape", "iphone_edgetoedge_landscape", "controller_landscape", "iphone_landscape", "skin_landscape", "background_landscape", "gamepad_landscape", "landscape"]
            : ["controller_edgetoedge_portrait", "iphone_edgetoedge_portrait", "controller_portrait", "iphone_portrait", "skin_portrait", "background_portrait", "gamepad_portrait", "portrait"]
        let sharedCandidates = ["controller", "skin", "background", "gamepad", "full", "layout"]

        for baseName in orientationCandidates + sharedCandidates {
            if let image = customImage(named: "\(baseName).png", baseName: baseName, directory: directory) {
                return image
            }
        }

        return nil
    }

    static func gameplayFullSkinImage(skin: VirtualPadSkin, isLandscape: Bool) -> UIImage? {
        guard skin == .custom, let directory = VirtualPadSkin.customSkinDirectory() else {
            return nil
        }

        // Manic/Delta-style full-phone skins include their own game viewport and
        // need info.json coordinates before we can place them accurately. For
        // gameplay, only use simple pad-area skins; otherwise fall back to the
        // built-in ARMSX2 controls so inputs never become visually misleading.
        guard !isLandscape else {
            return nil
        }

        let candidates = [
            "controller_portrait",
            "skin_portrait",
            "background_portrait",
            "gamepad_portrait",
            "portrait",
            "controller",
            "skin",
            "background",
            "gamepad",
            "full",
            "layout"
        ]

        for baseName in candidates {
            if let image = customImage(named: "\(baseName).png", baseName: baseName, directory: directory),
               !looksLikeEdgeToEdgePortrait(image) {
                return image
            }
        }

        return nil
    }

    static func gameplayFullSkinImage(
        descriptor: VPadSkinDescriptor,
        isLandscape: Bool,
        skinLibrary: VPadSkinLibraryStore = .shared
    ) -> UIImage? {
        let skin = descriptor.virtualPadSkin
        let directory: URL?
        if descriptor.source == .imported {
            directory = skinLibrary.importedAssetsDirectory(for: descriptor)
        } else if isLegacyCustomDescriptor(descriptor) {
            directory = VirtualPadSkin.legacyCustomSkinDirectory()
        } else {
            directory = nil
        }
        guard let directory else {
            return nil
        }

        guard !isLandscape else {
            return nil
        }

        let portraitCandidates = [
            "controller_portrait",
            "skin_portrait",
            "background_portrait",
            "gamepad_portrait",
            "portrait",
            "controller",
            "skin",
            "background",
            "gamepad",
            "full",
            "layout"
        ]

        for baseName in portraitCandidates {
            if let image = customImage(named: "\(baseName).png", baseName: baseName, directory: directory),
               !looksLikeEdgeToEdgePortrait(image) {
                return image
            }
        }

        return nil
    }

    private static func isLegacyCustomDescriptor(_ descriptor: VPadSkinDescriptor) -> Bool {
        descriptor.source != .imported && descriptor.id == VirtualPadSkin.custom.descriptorID
    }

    private static func looksLikeEdgeToEdgePortrait(_ image: UIImage) -> Bool {
        let aspect = image.size.height / max(image.size.width, 1)
        return aspect >= edgeToEdgePortraitAspectRatio
    }

    private static func customImage(named fileName: String, baseName: String, directory: URL) -> UIImage? {
        let candidates = [
            fileName,
            "\(baseName).png",
            "\(baseName).jpg",
            "\(baseName).jpeg",
            "\(baseName).webp"
        ]

        for candidate in candidates {
            let url = directory.appendingPathComponent(candidate)
            if FileManager.default.fileExists(atPath: url.path),
               let image = UIImage(contentsOfFile: url.path) {
                return image
            }
        }

        return nil
    }
}
