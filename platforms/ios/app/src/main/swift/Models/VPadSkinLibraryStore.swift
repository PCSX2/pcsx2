// VPadSkinLibraryStore.swift - JSON-backed virtual pad skin library
// SPDX-License-Identifier: GPL-3.0+

import Foundation
import SwiftUI
import UIKit

enum VPadSkinSource: String, Codable, Equatable {
    case builtIn
    case imported
}

struct VPadSkinDescriptor: Codable, Equatable, Identifiable {
    var id: String
    var displayName: String
    var source: VPadSkinSource
    var storageFolderName: String?
    var linkedLayoutPresetID: String?
    var manifestVersion: Int?
    var originalImportName: String?
    var builtInSkinRawValue: Int?
    var createdAt: Date
    var updatedAt: Date

    init(
        id: String,
        displayName: String,
        source: VPadSkinSource,
        storageFolderName: String? = nil,
        linkedLayoutPresetID: String? = nil,
        manifestVersion: Int? = nil,
        originalImportName: String? = nil,
        builtInSkinRawValue: Int? = nil,
        createdAt: Date = Date(),
        updatedAt: Date = Date()
    ) {
        self.id = id
        self.displayName = displayName
        self.source = source
        self.storageFolderName = storageFolderName
        self.linkedLayoutPresetID = linkedLayoutPresetID
        self.manifestVersion = manifestVersion
        self.originalImportName = originalImportName
        self.builtInSkinRawValue = builtInSkinRawValue
        self.createdAt = createdAt
        self.updatedAt = updatedAt
    }

    var isImported: Bool {
        source == .imported
    }

    var virtualPadSkin: VirtualPadSkin {
        if source == .builtIn,
           let raw = builtInSkinRawValue,
           let skin = VirtualPadSkin(rawValue: raw) {
            return skin
        }
        return .custom
    }
}

struct VPadSkinImportResult {
    var descriptor: VPadSkinDescriptor
    var warnings: [String]
    var importedImageCount: Int
    var extractedFileCount: Int

    var includesLinkedLayout: Bool {
        descriptor.linkedLayoutPresetID != nil
    }

    var message: String {
        var lines = ["Imported '\(descriptor.displayName)'."]
        if includesLinkedLayout {
            lines.append("This skin includes a recommended layout.")
        }
        lines.append(contentsOf: warnings)
        return lines.joined(separator: "\n")
    }
}

private struct VPadSkinLibrary: Codable {
    var schemaVersion: Int
    var selectedSkinID: String
    var didMigrateLegacyCustomSkin: Bool
    var importedSkins: [VPadSkinDescriptor]
}

private struct VPadSkinManifest: Codable {
    var schemaVersion: Int?
    var name: String?
    var layout: String?
}

enum VPadSkinLibraryStoreError: Error {
    case missingSkin
    case noUsableSkinImages
}

@Observable
final class VPadSkinLibraryStore: @unchecked Sendable {
    static let shared = VPadSkinLibraryStore()
    static let schemaVersion = 1

    private let libraryURL: URL
    private let assetsRootURL: URL
    private let legacyCustomSkinURL: URL?
    private(set) var importedDescriptors: [VPadSkinDescriptor] = []
    private var didMigrateLegacyCustomSkin = false

    var selectedSkinID: String {
        didSet {
            if selectedSkinID != oldValue {
                persist()
            }
        }
    }

    var allDescriptors: [VPadSkinDescriptor] {
        Self.builtInDescriptors + importedDescriptors
    }

    var selectedDescriptor: VPadSkinDescriptor {
        descriptor(id: selectedSkinID) ?? Self.defaultDescriptor
    }

    var selectedImportedSkinDescriptor: VPadSkinDescriptor? {
        let descriptor = selectedDescriptor
        return descriptor.source == .imported ? descriptor : nil
    }

    init(
        libraryURL: URL? = nil,
        assetsRootURL: URL? = nil,
        legacyCustomSkinURL: URL? = nil,
        initialSelectedSkinID: String? = nil
    ) {
        let defaultRoot = Self.defaultRootURL()
        self.libraryURL = libraryURL ?? defaultRoot.appendingPathComponent("VPadSkins.json")
        self.assetsRootURL = assetsRootURL ?? defaultRoot.appendingPathComponent("VPadSkins", isDirectory: true)
        self.legacyCustomSkinURL = legacyCustomSkinURL ?? VirtualPadSkin.legacyCustomSkinDirectory()
        selectedSkinID = initialSelectedSkinID ?? Self.defaultDescriptor.id

        if let library = Self.loadLibrary(from: self.libraryURL) {
            importedDescriptors = library.importedSkins.filter { $0.source == .imported }
            didMigrateLegacyCustomSkin = library.didMigrateLegacyCustomSkin
            selectedSkinID = validSkinID(library.selectedSkinID)
        } else if let initialSelectedSkinID {
            selectedSkinID = initialSelectedSkinID == VirtualPadSkin.custom.descriptorID
                ? initialSelectedSkinID
                : validSkinID(initialSelectedSkinID)
        }
    }

    static var builtInDescriptors: [VPadSkinDescriptor] {
        VirtualPadSkin.builtInCases.map { skin in
            VPadSkinDescriptor(
                id: skin.descriptorID,
                displayName: skin.label,
                source: .builtIn,
                storageFolderName: skin.bundledDirectoryName,
                builtInSkinRawValue: skin.rawValue
            )
        }
    }

    static var defaultDescriptor: VPadSkinDescriptor {
        VPadSkinDescriptor(
            id: VirtualPadSkin.armsx2Refresh.descriptorID,
            displayName: VirtualPadSkin.armsx2Refresh.label,
            source: .builtIn,
            builtInSkinRawValue: VirtualPadSkin.armsx2Refresh.rawValue
        )
    }

    func descriptor(id: String?) -> VPadSkinDescriptor? {
        guard let id else { return nil }
        return allDescriptors.first { $0.id == id }
    }

    func selectSkin(id: String) {
        selectedSkinID = validSkinID(id)
    }

    func importedAssetsDirectory(for descriptor: VPadSkinDescriptor) -> URL? {
        guard descriptor.source == .imported,
              let folder = descriptor.storageFolderName,
              !folder.isEmpty else {
            return nil
        }
        return assetsRootURL.appendingPathComponent(folder, isDirectory: true)
    }

    func selectedImportedAssetsDirectory() -> URL? {
        guard let descriptor = selectedImportedSkinDescriptor else {
            return nil
        }
        return importedAssetsDirectory(for: descriptor)
    }

    func adoptLegacySelection(_ skin: VirtualPadSkin) {
        guard selectedSkinID == Self.defaultDescriptor.id else { return }
        if skin == .custom {
            migrateLegacyCustomSkinIfNeeded()
        } else {
            selectedSkinID = skin.descriptorID
        }
    }

    func migrateLegacyCustomSkinIfNeeded() {
        guard !didMigrateLegacyCustomSkin else { return }
        defer {
            didMigrateLegacyCustomSkin = true
            persist()
        }

        guard let legacyCustomSkinURL,
              directoryContainsFiles(legacyCustomSkinURL) else {
            return
        }

        let id = "imported-\(UUID().uuidString)"
        let folder = id
        let destination = assetsRootURL.appendingPathComponent(folder, isDirectory: true)
        do {
            try FileManager.default.createDirectory(at: destination, withIntermediateDirectories: true)
            try copyDirectoryContents(from: legacyCustomSkinURL, to: destination)
            var descriptor = VPadSkinDescriptor(
                id: id,
                displayName: uniqueDisplayName("Custom Imported"),
                source: .imported,
                storageFolderName: folder,
                originalImportName: "Custom"
            )
            descriptor.createdAt = Date()
            descriptor.updatedAt = descriptor.createdAt
            importedDescriptors.append(descriptor)
            selectedSkinID = descriptor.id
        } catch {
            return
        }
    }

    @discardableResult
    func importSkin(
        from sourceURL: URL,
        originalImportName: String? = nil,
        layoutPresets: PadLayoutPresetStore
    ) throws -> VPadSkinImportResult {
        // Additive v2 manifest detection. A package carrying a valid v2 manifest
        // (info.json or a v2 manifest.json) is imported as an advanced manifest
        // skin; everything else falls through to the legacy importer below.
        switch importV2ManifestSkin(from: sourceURL, originalImportName: originalImportName) {
        case .notV2:
            break
        case .imported(let outcome):
            return outcome.asImportResult()
        case .failed(let error):
            throw error
        }
        let now = Date()
        let files = skinImportFiles(from: sourceURL)
        let manifest = manifest(in: files)
        let baseName = sanitizedDisplayName(
            manifest?.name,
            fallback: sanitizedDisplayName(
                sourceName(from: originalImportName ?? sourceURL.lastPathComponent),
                fallback: "Imported Skin"
            )
        )
        let displayName = uniqueDisplayName(baseName)
        let id = "imported-\(UUID().uuidString)"
        let destinationFolder = id
        let destination = assetsRootURL.appendingPathComponent(destinationFolder, isDirectory: true)
        try FileManager.default.createDirectory(at: destination, withIntermediateDirectories: true)

        let maxImageBytes: Int64 = 16 * 1024 * 1024
        let maxImportedImages: Int = 64

        var importedImageCount = 0
        var warnings: [String] = []
        for fileURL in files where Self.isSupportedImageFile(fileURL) {
            if importedImageCount >= maxImportedImages {
                warnings.append("Skipped remaining images; the import limit was reached.")
                break
            }
            guard let destinationName = Self.destinationSkinFileName(for: fileURL) else {
                warnings.append("Skipped \(fileURL.lastPathComponent).")
                continue
            }

            let fileSize = ((try? FileManager.default.attributesOfItem(atPath: fileURL.path))?[.size] as? NSNumber)?.int64Value ?? 0
            if fileSize > maxImageBytes {
                warnings.append("Skipped large image \(fileURL.lastPathComponent).")
                continue
            }

            guard let image = UIImage(contentsOfFile: fileURL.path),
                  let pngData = image.pngData() else {
                warnings.append("Skipped invalid image \(fileURL.lastPathComponent).")
                continue
            }

            try writeSkinAsset(pngData, to: destination.appendingPathComponent(destinationName))
            importedImageCount += 1
        }

        guard importedImageCount > 0 else {
            try? FileManager.default.removeItem(at: destination)
            throw VPadSkinLibraryStoreError.noUsableSkinImages
        }

        var descriptor = VPadSkinDescriptor(
            id: id,
            displayName: displayName,
            source: .imported,
            storageFolderName: destinationFolder,
            manifestVersion: manifest?.schemaVersion,
            originalImportName: originalImportName ?? sourceURL.lastPathComponent,
            createdAt: now,
            updatedAt: now
        )

        if let layoutName = manifest?.layout?.trimmingCharacters(in: .whitespacesAndNewlines),
           !layoutName.isEmpty {
            if let layoutURL = files.first(where: { $0.lastPathComponent.caseInsensitiveCompare(URL(fileURLWithPath: layoutName).lastPathComponent) == .orderedSame }) {
                do {
                    let data = try Data(contentsOf: layoutURL)
                    let snapshot = try Self.decodeImportedLayoutSnapshot(from: data)
                    let preset = layoutPresets.createPreset(
                        named: "\(displayName) Layout",
                        snapshot: snapshot,
                        source: .futureImportedSkin,
                        linkedSkinID: descriptor.id
                    )
                    descriptor.linkedLayoutPresetID = preset.id
                } catch {
                    warnings.append("Skipped invalid recommended layout.")
                }
            } else {
                warnings.append("Skipped missing recommended layout.")
            }
        }

        importedDescriptors.append(descriptor)
        persist()
        return VPadSkinImportResult(
            descriptor: descriptor,
            warnings: warnings,
            importedImageCount: importedImageCount,
            extractedFileCount: files.count
        )
    }

    @discardableResult
    func importSkinArchive(
        from sourceURL: URL,
        layoutPresets: PadLayoutPresetStore
    ) throws -> VPadSkinImportResult {
        let accessGranted = sourceURL.startAccessingSecurityScopedResource()
        defer {
            if accessGranted {
                sourceURL.stopAccessingSecurityScopedResource()
            }
        }

        let stagingDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("ARMSX2SkinImport-\(UUID().uuidString)", isDirectory: true)
        let archiveDirectory = stagingDirectory.appendingPathComponent("Package", isDirectory: true)
        try FileManager.default.createDirectory(at: stagingDirectory, withIntermediateDirectories: true)
        defer {
            try? FileManager.default.removeItem(at: stagingDirectory)
        }

        let isV2Package = SkinManifestImporter.shouldTreatAsV2(
            manifestData: ARMSX2Bridge.peekSkinManifestData(at: sourceURL)
        )
        let extracted = isV2Package
            ? ARMSX2Bridge.extractSkinPackageArchive(at: sourceURL, to: archiveDirectory)
            : ARMSX2Bridge.extractControllerSkinArchive(at: sourceURL, to: archiveDirectory)
        guard !extracted.isEmpty else {
            throw VPadSkinLibraryStoreError.noUsableSkinImages
        }

        return try importSkin(
            from: archiveDirectory,
            originalImportName: sourceURL.lastPathComponent,
            layoutPresets: layoutPresets
        )
    }

    private func importV2ManifestSkin(from sourceURL: URL, originalImportName: String?) -> SkinManifestImporter.V2ImportDecision {
        switch SkinManifestImporter.detectPackage(sourceURL: sourceURL) {
        case .legacy:
            return .notV2
        case .invalidV2(let message):
            return .failed(.invalidManifest(message: message))
        case .v2(let manifest, let packageRoot, let repairedParentFolder):
            let validation = SkinManifestImporter.validateForInstall(manifest, packageRoot: packageRoot)
            if !validation.errors.isEmpty {
                return .failed(.invalidManifest(message: SkinManifestImporter.summarizeErrors(validation.errors)))
            }

            let displayName = uniqueDisplayName(
                sanitizedDisplayName(manifest.name, fallback: originalImportName ?? sourceURL.lastPathComponent)
            )
            let id = "imported-\(UUID().uuidString)"
            let destination = assetsRootURL.appendingPathComponent(id, isDirectory: true)

            do {
                let manifestURL = SkinManifestImporter.manifestFile(in: packageRoot)
                let manifestData = manifestURL.flatMap { try? Data(contentsOf: $0) }
                let install = try SkinManifestImporter.installValidatedPackage(
                    manifest: manifest,
                    manifestData: manifestData,
                    packageRoot: packageRoot,
                    destination: destination,
                    maxFileBytes: 16 * 1024 * 1024,
                    maxFiles: 128
                )

                let now = Date()
                let descriptor = VPadSkinDescriptor(
                    id: id,
                    displayName: displayName,
                    source: .imported,
                    storageFolderName: id,
                    manifestVersion: 2,
                    originalImportName: originalImportName ?? sourceURL.lastPathComponent,
                    createdAt: now,
                    updatedAt: now
                )
                importedDescriptors.append(descriptor)
                persist()

                let reservedWarnings = validation.warnings.filter { SkinManifestImporter.isReservedFieldWarning($0) }
                let otherWarnings = validation.warnings.filter { !SkinManifestImporter.isReservedFieldWarning($0) }
                var warnings = SkinManifestImporter.successLines(
                    repaired: repairedParentFolder,
                    reservedWarnings: reservedWarnings,
                    author: manifest.author,
                    version: manifest.version
                )
                warnings.append(contentsOf: otherWarnings)

                let outcome = SkinManifestImporter.V2ImportOutcome(
                    descriptor: descriptor,
                    warnings: warnings,
                    manifestName: manifest.name,
                    manifestIdentifier: manifest.identifier,
                    manifestVersion: manifest.version,
                    manifestAuthor: manifest.author,
                    copiedAssetCount: install.copiedAssetCount,
                    repairedParentFolder: repairedParentFolder
                )
                return .imported(outcome)
            } catch {
                try? FileManager.default.removeItem(at: destination)
                return .failed(.storageFailed(message: error.localizedDescription))
            }
        }
    }

    func renameImportedSkin(id: String, to name: String) throws {
        guard let index = importedDescriptors.firstIndex(where: { $0.id == id }) else {
            throw VPadSkinLibraryStoreError.missingSkin
        }
        let existingName = importedDescriptors[index].displayName
        importedDescriptors[index].displayName = uniqueDisplayName(
            sanitizedDisplayName(name, fallback: existingName),
            ignoringSkinID: id
        )
        importedDescriptors[index].updatedAt = Date()
        persist()
    }

    func deleteImportedSkin(id: String, layoutPresets: PadLayoutPresetStore = .shared) throws {
        guard let index = importedDescriptors.firstIndex(where: { $0.id == id }) else {
            throw VPadSkinLibraryStoreError.missingSkin
        }
        let descriptor = importedDescriptors.remove(at: index)
        if selectedSkinID == id {
            selectedSkinID = Self.defaultDescriptor.id
        }
        layoutPresets.clearSkinAssignments(forSkinID: id)
        if let directory = importedAssetsDirectory(for: descriptor) {
            try? FileManager.default.removeItem(at: directory)
        }
        persist()
    }

    func save() throws {
        try FileManager.default.createDirectory(at: libraryURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        let library = VPadSkinLibrary(
            schemaVersion: Self.schemaVersion,
            selectedSkinID: validSkinID(selectedSkinID),
            didMigrateLegacyCustomSkin: didMigrateLegacyCustomSkin,
            importedSkins: importedDescriptors
        )
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        encoder.dateEncodingStrategy = .iso8601
        try encoder.encode(library).write(to: libraryURL, options: .atomic)
    }

    static func decodeImportedLayoutSnapshot(from data: Data) throws -> PadLayoutSnapshot {
        try PadLayoutImportExport.decodeSnapshot(from: data)
    }

    static func canonicalSkinFileName(forImportPath path: String) -> String? {
        let url = URL(fileURLWithPath: path)
        let exact = url.lastPathComponent.lowercased()
        let expected = Set([
            "ic_controller_up_button.png",
            "ic_controller_down_button.png",
            "ic_controller_left_button.png",
            "ic_controller_right_button.png",
            "ic_controller_cross_button.png",
            "ic_controller_circle_button.png",
            "ic_controller_square_button.png",
            "ic_controller_triangle_button.png",
            "ic_controller_l1_button.png",
            "ic_controller_r1_button.png",
            "ic_controller_l2_button.png",
            "ic_controller_r2_button.png",
            "ic_controller_start_button.png",
            "ic_controller_select_button.png",
            "ic_controller_l3_button.png",
            "ic_controller_r3_button.png",
            "ic_controller_analog_base_left.png",
            "ic_controller_analog_base_right.png",
            "ic_controller_analog_base.png",
            "ic_controller_analog_stick.png",
            "ic_controller_analog_button.png",
            "ic_controller_analog_stick_left.png",
            "ic_controller_analog_stick_right.png",
            "ic_controller_analog_button_left.png",
            "ic_controller_analog_button_right.png"
        ])
        if expected.contains(exact) {
            return exact
        }

        let stem = url.deletingPathExtension().lastPathComponent
            .lowercased()
            .replacingOccurrences(of: "[^a-z0-9]+", with: "_", options: .regularExpression)

        let pairs: [(String, String)] = [
            ("analog_base_left", "ic_controller_analog_base_left.png"),
            ("analog_base_right", "ic_controller_analog_base_right.png"),
            ("left_analog_base", "ic_controller_analog_base_left.png"),
            ("right_analog_base", "ic_controller_analog_base_right.png"),
            ("analog_base", "ic_controller_analog_base.png"),
            ("analog_stick_left", "ic_controller_analog_stick_left.png"),
            ("analog_stick_right", "ic_controller_analog_stick_right.png"),
            ("analog_button_left", "ic_controller_analog_button_left.png"),
            ("analog_button_right", "ic_controller_analog_button_right.png"),
            ("analog_stick", "ic_controller_analog_stick.png"),
            ("analog_button", "ic_controller_analog_button.png"),
            ("stick", "ic_controller_analog_stick.png"),
            ("base", "ic_controller_analog_base.png"),
            ("l1", "ic_controller_l1_button.png"),
            ("r1", "ic_controller_r1_button.png"),
            ("l2", "ic_controller_l2_button.png"),
            ("r2", "ic_controller_r2_button.png"),
            ("l3", "ic_controller_l3_button.png"),
            ("r3", "ic_controller_r3_button.png"),
            ("start", "ic_controller_start_button.png"),
            ("select", "ic_controller_select_button.png"),
            ("triangle", "ic_controller_triangle_button.png"),
            ("circle", "ic_controller_circle_button.png"),
            ("square", "ic_controller_square_button.png"),
            ("cross", "ic_controller_cross_button.png"),
            ("up", "ic_controller_up_button.png"),
            ("down", "ic_controller_down_button.png"),
            ("left", "ic_controller_left_button.png"),
            ("right", "ic_controller_right_button.png")
        ]

        return pairs.first { stem.contains($0.0) }?.1
    }

    private static func loadLibrary(from url: URL) -> VPadSkinLibrary? {
        guard FileManager.default.fileExists(atPath: url.path),
              let data = try? Data(contentsOf: url) else {
            return nil
        }
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        return try? decoder.decode(VPadSkinLibrary.self, from: data)
    }

    private func validSkinID(_ id: String) -> String {
        descriptor(id: id)?.id ?? Self.defaultDescriptor.id
    }

    private func uniqueDisplayName(_ name: String, ignoringSkinID: String? = nil) -> String {
        let base = sanitizedDisplayName(name, fallback: "Imported Skin")
        let used = Set(importedDescriptors.compactMap { descriptor -> String? in
            descriptor.id == ignoringSkinID ? nil : descriptor.displayName.lowercased()
        })
        if !used.contains(base.lowercased()) {
            return base
        }
        var suffix = 2
        while used.contains("\(base) \(suffix)".lowercased()) {
            suffix += 1
        }
        return "\(base) \(suffix)"
    }

    private func sanitizedDisplayName(_ name: String?, fallback: String) -> String {
        let trimmed = (name ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        return trimmed.isEmpty ? fallback : trimmed
    }

    private func sourceName(from fileName: String) -> String {
        let stem = URL(fileURLWithPath: fileName).deletingPathExtension().lastPathComponent
        return stem.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    private func skinImportFiles(from sourceURL: URL) -> [URL] {
        let resourceValues = try? sourceURL.resourceValues(forKeys: [.isDirectoryKey])
        if resourceValues?.isDirectory == true {
            let files = (try? FileManager.default.contentsOfDirectory(
                at: sourceURL,
                includingPropertiesForKeys: [.isRegularFileKey],
                options: [.skipsHiddenFiles]
            )) ?? []
            return files.filter { url in
                let values = try? url.resourceValues(forKeys: [.isRegularFileKey])
                return values?.isRegularFile != false
            }
        }
        return [sourceURL]
    }

    private func manifest(in files: [URL]) -> VPadSkinManifest? {
        guard let manifestURL = files.first(where: { $0.lastPathComponent.caseInsensitiveCompare("manifest.json") == .orderedSame }),
              let data = try? Data(contentsOf: manifestURL) else {
            return nil
        }
        return try? JSONDecoder().decode(VPadSkinManifest.self, from: data)
    }

    private func copySkinAsset(from sourceURL: URL, to destinationURL: URL) throws {
        let data = try Data(contentsOf: sourceURL)
        try writeSkinAsset(data, to: destinationURL)
    }

    private func writeSkinAsset(_ data: Data, to destinationURL: URL) throws {
        try FileManager.default.createDirectory(at: destinationURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        if FileManager.default.fileExists(atPath: destinationURL.path) {
            try FileManager.default.removeItem(at: destinationURL)
        }
        try data.write(to: destinationURL, options: .atomic)
    }

    private func copyDirectoryContents(from source: URL, to destination: URL) throws {
        let files = (try? FileManager.default.contentsOfDirectory(
            at: source,
            includingPropertiesForKeys: [.isRegularFileKey],
            options: [.skipsHiddenFiles]
        )) ?? []
        for file in files {
            let values = try? file.resourceValues(forKeys: [.isRegularFileKey])
            guard values?.isRegularFile != false else { continue }
            try copySkinAsset(from: file, to: destination.appendingPathComponent(file.lastPathComponent))
        }
    }

    private func directoryContainsFiles(_ url: URL) -> Bool {
        let files = try? FileManager.default.contentsOfDirectory(at: url, includingPropertiesForKeys: [.isRegularFileKey])
        return files?.contains { file in
            let values = try? file.resourceValues(forKeys: [.isRegularFileKey])
            return values?.isRegularFile == true
        } ?? false
    }

    private func persist() {
        do {
            try save()
        } catch {
            NSLog("[ARMSX2 iOS Skins] Failed to save skin library: %@", error.localizedDescription)
        }
    }

    private static func destinationSkinFileName(for url: URL) -> String? {
        if let canonical = canonicalSkinFileName(forImportPath: url.path) {
            return canonical
        }
        return fullSkinFileName(for: url)
    }

    private static func fullSkinFileName(for url: URL) -> String? {
        let stem = url.deletingPathExtension().lastPathComponent
            .lowercased()
            .replacingOccurrences(of: "[^a-z0-9]+", with: "_", options: .regularExpression)
        let looksLikeDeviceSkin = stem.contains("edge") || stem.contains("iphone") || stem.contains("ipad")

        if stem.contains("landscape") || stem.contains("horizontal") {
            return looksLikeDeviceSkin ? "controller_edgetoedge_landscape.png" : "controller_landscape.png"
        }
        if stem.contains("portrait") || stem.contains("vertical") {
            return looksLikeDeviceSkin ? "controller_edgetoedge_portrait.png" : "controller_portrait.png"
        }

        if let image = UIImage(contentsOfFile: url.path) {
            return image.size.width > image.size.height ? "controller_landscape.png" : "controller_portrait.png"
        }
        return nil
    }

    private static func isSupportedImageFile(_ url: URL) -> Bool {
        switch url.pathExtension.lowercased() {
        case "png", "jpg", "jpeg", "webp":
            return true
        default:
            return false
        }
    }

    private static func defaultRootURL() -> URL {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first
            ?? FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first
            ?? FileManager.default.temporaryDirectory
        return base.appendingPathComponent("ARMSX2", isDirectory: true)
    }
}
