// InitialContentBootstrap.swift — Security-scoped ARMSX2 folder import
// SPDX-License-Identifier: GPL-3.0+

import Foundation

@MainActor
@Observable
final class InitialContentBootstrap {
    static let shared = InitialContentBootstrap()
    nonisolated static let didChangeNotification = Notification.Name("ARMSX2iOSInitialContentDidChange")

    private static let bookmarkKey = "ARMSX2iOSExternalRootBookmark"
    private static let displayNameKey = "ARMSX2iOSExternalRootDisplayName"

    private(set) var selectedFolderName: String?
    private(set) var isRunning = false

    var hasSelectedFolder: Bool {
        UserDefaults.standard.data(forKey: Self.bookmarkKey) != nil
    }

    private init() {
        selectedFolderName = UserDefaults.standard.string(forKey: Self.displayNameKey)
    }

    /// Saves the access granted by UIDocumentPickerViewController and immediately
    /// scans the selected ARMSX2 root for BIOS, GAMES, PRESETS, and SKINS.
    func selectARMSX2Folder(_ url: URL) async -> String {
        guard !isRunning else {
            return "The selected ARMSX2 folder is already being scanned."
        }
        isRunning = true
        defer { isRunning = false }

        // Security scope belongs to the exact URL returned by the picker.
        // Do not standardize or reconstruct it before requesting access.
        let selectedURL = url
        let accessing = selectedURL.startAccessingSecurityScopedResource()
        defer {
            if accessing {
                selectedURL.stopAccessingSecurityScopedResource()
            }
        }

        do {
            let values = try selectedURL.resourceValues(forKeys: [.isDirectoryKey])
            guard values.isDirectory == true else {
                throw InitialContentAccessError.notDirectory
            }
            _ = try FileManager.default.contentsOfDirectory(
                at: selectedURL,
                includingPropertiesForKeys: nil,
                options: [.skipsHiddenFiles]
            )

            try saveBookmark(for: selectedURL)
            let displayName = selectedURL.lastPathComponent.isEmpty
                ? "ARMSX2"
                : selectedURL.lastPathComponent
            UserDefaults.standard.set(displayName, forKey: Self.displayNameKey)
            selectedFolderName = displayName

            return await importContents(from: selectedURL)
        } catch {
            NSLog(
                "[ARMSX2 iOS Folder] selection failed path=%@ securityScoped=%d error=%@",
                selectedURL.path,
                accessing ? 1 : 0,
                error.localizedDescription
            )
            return "ARMSX2 folder access failed: \(error.localizedDescription)"
        }
    }

    /// Resolves the saved bookmark and scans while its security scope is active.
    func scanSelectedFolder() async -> String {
        guard !isRunning else {
            return "The selected ARMSX2 folder is already being scanned."
        }
        guard let bookmarkData = UserDefaults.standard.data(forKey: Self.bookmarkKey) else {
            return "Select the ARMSX2 folder first."
        }

        isRunning = true
        defer { isRunning = false }

        do {
            var isStale = false
            // As with the picker URL, retain the resolved URL instance carrying
            // the bookmark's scope instead of replacing it with a standardized URL.
            let rootURL = try URL(
                resolvingBookmarkData: bookmarkData,
                options: bookmarkResolutionOptions,
                relativeTo: nil,
                bookmarkDataIsStale: &isStale
            )

            let accessing = rootURL.startAccessingSecurityScopedResource()
            defer {
                if accessing {
                    rootURL.stopAccessingSecurityScopedResource()
                }
            }

            let values = try rootURL.resourceValues(forKeys: [.isDirectoryKey])
            guard values.isDirectory == true else {
                throw InitialContentAccessError.notDirectory
            }
            _ = try FileManager.default.contentsOfDirectory(
                at: rootURL,
                includingPropertiesForKeys: nil,
                options: [.skipsHiddenFiles]
            )

            if isStale {
                try saveBookmark(for: rootURL)
                NSLog("[ARMSX2 iOS Folder] refreshed stale bookmark path=%@", rootURL.path)
            }

            return await importContents(from: rootURL)
        } catch {
            NSLog("[ARMSX2 iOS Folder] bookmark scan failed error=%@", error.localizedDescription)
            return "The saved ARMSX2 folder could not be opened. Select it again to renew access.\n\(error.localizedDescription)"
        }
    }

    func removeSelectedFolder() {
        let defaults = UserDefaults.standard
        defaults.removeObject(forKey: Self.bookmarkKey)
        defaults.removeObject(forKey: Self.displayNameKey)
        selectedFolderName = nil
        NSLog("[ARMSX2 iOS Folder] removed saved folder access")
    }

    private var bookmarkCreationOptions: URL.BookmarkCreationOptions {
#if targetEnvironment(macCatalyst)
        [.withSecurityScope]
#else
        // withSecurityScope is unavailable on iOS. A URL returned by the
        // document picker carries its security scope in regular bookmark data.
        // Keep the full bookmark payload for the most reliable provider lookup.
        []
#endif
    }

    private var bookmarkResolutionOptions: URL.BookmarkResolutionOptions {
#if targetEnvironment(macCatalyst)
        [.withSecurityScope, .withoutUI]
#else
        [.withoutUI]
#endif
    }

    private func saveBookmark(for url: URL) throws {
        let bookmarkData = try url.bookmarkData(
            options: bookmarkCreationOptions,
            includingResourceValuesForKeys: nil,
            relativeTo: nil
        )
        UserDefaults.standard.set(bookmarkData, forKey: Self.bookmarkKey)
    }

    private func importContents(from rootDirectory: URL) async -> String {
        let biosDirectory = URL(
            fileURLWithPath: ARMSX2Bridge.biosDirectory(),
            isDirectory: true
        )
        let gameDirectory = URL(
            fileURLWithPath: ARMSX2Bridge.isoDirectory(),
            isDirectory: true
        )

        let preparation = await Task.detached(priority: .utility) {
            InitialContentFileInstaller.prepare(
                rootDirectory: rootDirectory,
                biosDirectory: biosDirectory
            )
        }.value

        configureDefaultBIOS()
        let appliedPresets = applyPresetFiles(named: preparation.presetNames)
        let skinImport = importSkinArchives(preparation.skinArchives)
        NotificationCenter.default.post(name: Self.didChangeNotification, object: nil)

        let gameImport = await Task.detached(priority: .utility) {
            InitialContentFileInstaller.importGames(
                preparation.gameFiles,
                into: gameDirectory
            )
        }.value
        NotificationCenter.default.post(name: Self.didChangeNotification, object: nil)
        let coverDownload = await downloadMissingCovers(for: gameImport.importedGameFiles)
        if coverDownload.downloaded > 0 {
            NotificationCenter.default.post(name: Self.didChangeNotification, object: nil)
        }

        NSLog(
            "[ARMSX2 iOS Folder] complete root=%@ BIOS candidates=%d copied=%d games discovered=%d imported=%d skipped=%d failed=%d covers=%d skins=%d skinSkipped=%d skinFailed=%d defaultSkin=%@ presets=%@",
            rootDirectory.path,
            preparation.biosCandidates,
            preparation.biosCopied,
            preparation.gameFiles.count,
            gameImport.imported,
            gameImport.skipped,
            gameImport.failed,
            coverDownload.downloaded,
            skinImport.imported,
            skinImport.skipped,
            skinImport.failed,
            skinImport.selectedDefaultName ?? "",
            appliedPresets.joined(separator: ", ")
        )

        var summary = [
            "ARMSX2 folder scan complete.",
            "BIOS: \(preparation.biosCopied) imported, \(preparation.biosCandidates) found.",
            "Games: \(gameImport.imported) imported, \(gameImport.skipped) already present.",
            "Covers: \(coverDownload.downloaded) downloaded for newly imported games.",
            "Skins: \(skinImport.imported) imported, \(skinImport.skipped) already present."
        ]
        if gameImport.failed > 0 {
            summary.append("Games that could not be imported: \(gameImport.failed).")
        }
        if !appliedPresets.isEmpty {
            summary.append("Presets applied: \(appliedPresets.joined(separator: ", ")).")
        } else {
            summary.append("No matching built-in preset files were found.")
        }
        if skinImport.failed > 0 {
            summary.append("Skins that could not be imported: \(skinImport.failed).")
        }
        if let selectedDefaultName = skinImport.selectedDefaultName {
            summary.append("Default skin and layout: \(selectedDefaultName).")
        }
        return summary.joined(separator: "\n")
    }

    private func configureDefaultBIOS() {
        let validBIOSes = ARMSX2Bridge.availableBIOSInfos().filter(\.valid)
        guard let firstValidBIOS = validBIOSes.first else { return }

        let currentDefault = ARMSX2Bridge.defaultBIOSName()
        guard !validBIOSes.contains(where: { $0.fileName == currentDefault }) else { return }
        ARMSX2Bridge.setDefaultBIOS(firstValidBIOS.fileName)
    }

    private func applyPresetFiles(named normalizedNames: Set<String>) -> [String] {
        var applied: [String] = []

        for preset in BuiltInSettingsPreset.allCases
        where normalizedNames.contains(InitialContentFileInstaller.normalizedPresetName(preset.rawValue)) {
            preset.apply()
            applied.append(preset.rawValue)
        }

        for preset in BuiltInDynamicControlPreset.allCases
        where normalizedNames.contains(InitialContentFileInstaller.normalizedPresetName(preset.rawValue)) {
            preset.apply()
            applied.append(preset.rawValue)
        }
        return applied
    }

    private func importSkinArchives(_ sourceURLs: [URL]) -> InitialSkinImportResult {
        let skinLibrary = VPadSkinLibraryStore.shared
        let layoutPresets = PadLayoutPresetStore.shared
        var result = InitialSkinImportResult()
        var defaultDescriptor: VPadSkinDescriptor?

        for sourceURL in sourceURLs.sorted(by: {
            $0.lastPathComponent.localizedStandardCompare($1.lastPathComponent) == .orderedAscending
        }) {
            let originalName = sourceURL.lastPathComponent
            let descriptor: VPadSkinDescriptor

            if let existing = skinLibrary.importedDescriptors.first(where: {
                $0.originalImportName?.caseInsensitiveCompare(originalName) == .orderedSame
            }) {
                descriptor = existing
                result.skipped += 1
            } else {
                do {
                    let importResult = try skinLibrary.importSkinArchive(
                        from: sourceURL,
                        layoutPresets: layoutPresets
                    )
                    descriptor = importResult.descriptor
                    result.imported += 1
                } catch {
                    result.failed += 1
                    NSLog(
                        "[ARMSX2 iOS Folder] skin import failed source=%@ error=%@",
                        sourceURL.path,
                        error.localizedDescription
                    )
                    continue
                }
            }

            if defaultDescriptor == nil,
               sourceURL.deletingPathExtension().lastPathComponent.hasPrefix("1") {
                defaultDescriptor = descriptor
            }
        }

        if let defaultDescriptor {
            skinLibrary.selectSkin(id: defaultDescriptor.id)
            SettingsStore.shared.virtualPadSkin = defaultDescriptor.virtualPadSkin
            layoutPresets.globalPresetID = defaultDescriptor.linkedLayoutPresetID
            result.selectedDefaultName = defaultDescriptor.displayName
        }
        return result
    }

    private func downloadMissingCovers(for importedGameFiles: [URL]) async -> CoverDownloadSummary {
        guard !importedGameFiles.isEmpty else {
            return CoverDownloadSummary(downloaded: 0, skippedExisting: 0, failed: 0)
        }

        let coverStore = CoverStore.shared
        let targets = importedGameFiles.map { fileURL in
            let name = fileURL.lastPathComponent
            let metadata = ARMSX2Bridge.gameMetadata(forISO: name)
            let existingCover = coverStore.coverURL(
                forGameName: name,
                gamePath: fileURL,
                metadata: metadata
            )
            return CoverGameInfo(
                name: name,
                fileURL: fileURL,
                metadata: metadata,
                hasCover: existingCover != nil
            )
        }
        return await coverStore.downloadMissingCovers(for: targets, showResult: false)
    }
}

private enum InitialContentAccessError: LocalizedError {
    case notDirectory

    var errorDescription: String? {
        switch self {
        case .notDirectory:
            return "The selected item is not a folder."
        }
    }
}

private struct InitialContentPreparation: Sendable {
    let biosCandidates: Int
    let biosCopied: Int
    let gameFiles: [URL]
    let presetNames: Set<String>
    let skinArchives: [URL]
}

private struct InitialGameImportResult: Sendable {
    var imported = 0
    var skipped = 0
    var failed = 0
    var importedGameFiles: [URL] = []
}

private struct InitialSkinImportResult {
    var imported = 0
    var skipped = 0
    var failed = 0
    var selectedDefaultName: String?
}

private enum InitialContentFileInstaller {
    private static let biosMaximumSize: UInt64 = 50 * 1024 * 1024
    private static let supportedGameExtensions = Set([
        "iso", "chd", "img", "bin", "cue", "mdf", "cso", "zso", "gz", "elf"
    ])

    static func prepare(
        rootDirectory: URL,
        biosDirectory: URL
    ) -> InitialContentPreparation {
        let fileManager = FileManager.default
        var searchRoots = [rootDirectory]
        searchRoots.append(contentsOf: existingDirectories(
            named: "ARMSX2",
            inside: rootDirectory,
            fileManager: fileManager
        ))

        let biosInputDirectories = inputDirectories(
            named: "BIOS",
            roots: searchRoots,
            fileManager: fileManager
        )
        let gameInputDirectories = inputDirectories(
            named: "GAMES",
            roots: searchRoots,
            fileManager: fileManager
        )
        let presetInputDirectories = inputDirectories(
            named: "PRESETS",
            roots: searchRoots,
            fileManager: fileManager
        )
        let skinInputDirectories = inputDirectories(
            named: "SKINS",
            roots: searchRoots,
            fileManager: fileManager
        )
        NSLog(
            "[ARMSX2 iOS Folder] scan BIOS=%@ GAMES=%@ PRESETS=%@ SKINS=%@",
            biosInputDirectories.map(\.path).joined(separator: ", "),
            gameInputDirectories.map(\.path).joined(separator: ", "),
            presetInputDirectories.map(\.path).joined(separator: ", "),
            skinInputDirectories.map(\.path).joined(separator: ", ")
        )

        var biosCandidates = 0
        var biosCopied = 0
        var seenBIOSPaths = Set<String>()
        try? fileManager.createDirectory(at: biosDirectory, withIntermediateDirectories: true)

        for inputDirectory in biosInputDirectories {
            for source in regularFiles(in: inputDirectory, fileManager: fileManager)
            where source.pathExtension.caseInsensitiveCompare("bin") == .orderedSame ||
                source.pathExtension.caseInsensitiveCompare("rom") == .orderedSame {
                let sourceKey = source.standardizedFileURL.path
                guard seenBIOSPaths.insert(sourceKey).inserted,
                      fileSize(of: source, fileManager: fileManager) <= biosMaximumSize else {
                    continue
                }

                biosCandidates += 1
                guard !sameDirectory(source.deletingLastPathComponent(), biosDirectory) else {
                    continue
                }

                let destination = biosDirectory.appendingPathComponent(source.lastPathComponent)
                guard !fileManager.fileExists(atPath: destination.path) else { continue }
                do {
                    try ImportFileCopier.copy(from: source, to: destination)
                    biosCopied += 1
                } catch {
                    NSLog(
                        "[ARMSX2 iOS Folder] BIOS import failed source=%@ error=%@",
                        source.path,
                        error.localizedDescription
                    )
                }
            }
        }

        var gameFiles: [URL] = []
        var seenGamePaths = Set<String>()
        for inputDirectory in gameInputDirectories {
            for source in regularFiles(in: inputDirectory, fileManager: fileManager)
            where isSupportedGame(source, fileManager: fileManager) {
                let sourceKey = source.standardizedFileURL.path
                if seenGamePaths.insert(sourceKey).inserted {
                    gameFiles.append(source)
                }
            }
        }

        var presetNames = Set<String>()
        var seenPresetPaths = Set<String>()
        for inputDirectory in presetInputDirectories {
            for source in regularFiles(in: inputDirectory, fileManager: fileManager)
            where source.pathExtension.caseInsensitiveCompare("ini") == .orderedSame {
                let sourceKey = source.standardizedFileURL.path
                guard seenPresetPaths.insert(sourceKey).inserted else { continue }
                presetNames.insert(
                    normalizedPresetName(source.deletingPathExtension().lastPathComponent)
                )
            }
        }

        var skinArchives: [URL] = []
        var seenSkinPaths = Set<String>()
        for inputDirectory in skinInputDirectories {
            for source in regularFiles(in: inputDirectory, fileManager: fileManager)
            where source.pathExtension.caseInsensitiveCompare("zip") == .orderedSame {
                let sourceKey = source.standardizedFileURL.path
                if seenSkinPaths.insert(sourceKey).inserted {
                    skinArchives.append(source)
                }
            }
        }

        return InitialContentPreparation(
            biosCandidates: biosCandidates,
            biosCopied: biosCopied,
            gameFiles: gameFiles,
            presetNames: presetNames,
            skinArchives: skinArchives
        )
    }

    static func importGames(_ sources: [URL], into destinationDirectory: URL) -> InitialGameImportResult {
        let fileManager = FileManager.default
        var result = InitialGameImportResult()
        try? fileManager.createDirectory(at: destinationDirectory, withIntermediateDirectories: true)

        for source in sources {
            let destination = destinationDirectory.appendingPathComponent(source.lastPathComponent)
            if source.standardizedFileURL == destination.standardizedFileURL ||
                fileManager.fileExists(atPath: destination.path) {
                result.skipped += 1
                continue
            }

            do {
                try ImportFileCopier.copy(from: source, to: destination)
                result.imported += 1
                result.importedGameFiles.append(destination)
            } catch {
                result.failed += 1
                NSLog(
                    "[ARMSX2 iOS Folder] game import failed source=%@ error=%@",
                    source.path,
                    error.localizedDescription
                )
            }
        }
        return result
    }

    static func normalizedPresetName(_ name: String) -> String {
        name
            .trimmingCharacters(in: .whitespacesAndNewlines)
            .precomposedStringWithCanonicalMapping
            .lowercased(with: Locale(identifier: "en_US_POSIX"))
    }

    private static func inputDirectories(
        named name: String,
        roots: [URL],
        fileManager: FileManager
    ) -> [URL] {
        var directories: [URL] = []
        var seenPaths = Set<String>()
        for root in roots {
            for directory in existingDirectories(
                named: name,
                inside: root,
                fileManager: fileManager
            ) {
                let key = directory.standardizedFileURL.path
                if seenPaths.insert(key).inserted {
                    directories.append(directory)
                }
            }
        }
        return directories
    }

    private static func existingDirectories(
        named name: String,
        inside root: URL,
        fileManager: FileManager
    ) -> [URL] {
        let children = try? fileManager.contentsOfDirectory(
            at: root,
            includingPropertiesForKeys: [.isDirectoryKey],
            options: [.skipsHiddenFiles]
        )
        return children?.filter { child in
            guard child.lastPathComponent.caseInsensitiveCompare(name) == .orderedSame else {
                return false
            }
            return (try? child.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) == true
        } ?? []
    }

    private static func regularFiles(in directory: URL, fileManager: FileManager) -> [URL] {
        guard let enumerator = fileManager.enumerator(
            at: directory,
            includingPropertiesForKeys: [.isRegularFileKey],
            options: [.skipsHiddenFiles, .skipsPackageDescendants]
        ) else {
            return []
        }

        var files: [URL] = []
        for case let url as URL in enumerator {
            if (try? url.resourceValues(forKeys: [.isRegularFileKey]).isRegularFile) == true {
                files.append(url)
            }
        }
        return files
    }

    private static func isSupportedGame(_ url: URL, fileManager: FileManager) -> Bool {
        let ext = url.pathExtension.lowercased()
        guard supportedGameExtensions.contains(ext) else { return false }
        if ext == "bin" {
            return fileSize(of: url, fileManager: fileManager) > biosMaximumSize
        }
        return true
    }

    private static func fileSize(of url: URL, fileManager: FileManager) -> UInt64 {
        let attributes = try? fileManager.attributesOfItem(atPath: url.path)
        return attributes?[.size] as? UInt64 ?? 0
    }

    private static func sameDirectory(_ lhs: URL, _ rhs: URL) -> Bool {
        lhs.standardizedFileURL.path == rhs.standardizedFileURL.path
    }
}
