// VirtualPadSkin.swift - virtual pad skin model for SwiftUI
// SPDX-License-Identifier: GPL-3.0+

import Foundation

enum VirtualPadSkin: Int, CaseIterable, Identifiable {
    case armsx2Refresh = 0
    case legacyRefresh = 3
    case fullWhite = 4
    case whiteDS = 5
    case whiteFullColorButton = 6
    case blackColored = 7
    case blackDS = 8
    case blackWhite = 9
    case liquidGlass = 10
    case black = 11
    case xbox = 12
    case crispVector = 1
    case custom = 2

    var id: Int { rawValue }

    static var builtInCases: [VirtualPadSkin] {
        allCases.filter { $0 != .custom }
    }

    var descriptorID: String {
        switch self {
        case .custom:
            return "legacy-custom"
        default:
            return "built-in-\(rawValue)"
        }
    }

    var label: String {
        switch self {
        case .armsx2Refresh:
            return "White Colored"
        case .crispVector:
            return "Crisp Vector"
        case .custom:
            return "Custom Imported"
        case .legacyRefresh:
            return "ARMSX2 Refresh Legacy"
        case .fullWhite:
            return "Full White"
        case .whiteDS:
            return "White DS"
        case .whiteFullColorButton:
            return "White Full Color Button"
        case .blackColored:
            return "Black Colored"
        case .blackDS:
            return "Black DS"
        case .blackWhite:
            return "Black White"
        case .liquidGlass:
            return "Liquid Glass"
        case .black:
            return "Black"
        case .xbox:
            return "Xbox"
        }
    }

    var detail: String {
        switch self {
        case .armsx2Refresh:
            return "Uses the bundled white controller art as the default on-screen pad."
        case .crispVector:
            return "Draws the pad in SwiftUI for sharper outlines at any screen scale."
        case .custom:
            return "Loads user-imported button images or a full portrait/landscape skin from the custom skin folder."
        case .legacyRefresh:
            return "Uses the previous ARMSX2 refresh controller art."
        case .fullWhite, .whiteDS, .whiteFullColorButton, .blackColored, .blackDS, .blackWhite, .liquidGlass, .black, .xbox:
            return "Uses a bundled controller skin."
        }
    }

    var bundledDirectoryName: String? {
        switch self {
        case .armsx2Refresh, .crispVector, .custom:
            return nil
        case .legacyRefresh:
            return "legacy_refresh"
        case .fullWhite:
            return "full_white"
        case .whiteDS:
            return "white_ds"
        case .whiteFullColorButton:
            return "white_full_color_button"
        case .blackColored:
            return "black_colored"
        case .blackDS:
            return "black_ds"
        case .blackWhite:
            return "black_white"
        case .liquidGlass:
            return "liquid_glass"
        case .black:
            return "black"
        case .xbox:
            return "xbox"
        }
    }

    static func customSkinDirectory(create: Bool = false) -> URL? {
        if let selectedDirectory = VPadSkinLibraryStore.shared.selectedImportedAssetsDirectory() {
            if create {
                try? FileManager.default.createDirectory(at: selectedDirectory, withIntermediateDirectories: true)
            }
            return selectedDirectory
        }

        return legacyCustomSkinDirectory(create: create)
    }

    static func legacyCustomSkinDirectory(create: Bool = false) -> URL? {
        guard let documents = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first else {
            return nil
        }

        let directory = documents.appendingPathComponent("ControllerSkins/Custom", isDirectory: true)
        if create {
            try? FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        }
        return directory
    }
}
