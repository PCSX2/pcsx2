// FileImportHandler.swift — Handle file import from Open-In / drag & drop
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import Foundation
import UniformTypeIdentifiers
import UIKit

@MainActor
@Observable
final class FileImportHandler {
    static let shared = FileImportHandler()

    struct ImportedGame: Sendable {
        let name: String
        let fileURL: URL
    }

    enum ImportDestination {
        case automatic
        case bios
        case game
        case pnachCheat
    }

    var lastImportMessage: String?
    var showImportAlert = false

    private static let biosExtensionList = ["bin", "rom"]
    private static let gameExtensionList = ["iso", "chd", "img", "bin", "cue", "mdf", "cso", "zso", "gz", "elf"]
    private static let pnachExtensionList = ["pnach"]
    private static let biosExtensions = Set(biosExtensionList)
    private static let gameExtensions = Set(gameExtensionList)
    private static let pnachExtensions = Set(pnachExtensionList)
    // .bin files > 50MB are treated as game images, not BIOS
    private static let biosSizeThreshold: UInt64 = 50 * 1024 * 1024

    // BIOS dumps use loose/non-standard UTTypes on iOS. Keep the picker permissive
    // but include explicit .bin/.rom types so sideloaded Files providers expose them.
    static let biosContentTypes: [UTType] = broaderContentTypes(for: biosExtensionList)
    static let gameContentTypes: [UTType] = broaderContentTypes(for: gameExtensionList)
    static let pnachContentTypes: [UTType] = Array(Set([.item, .data, .content, .text, .plainText] + contentTypes(for: pnachExtensionList + ["txt", "patch"])))
    static let pnachImportNeedsGameMessage = "PNACH patches need to be imported for a specific game. Boot a game first or long-press a game in your library, then import the patch."
    static let pnachCheatBlockedByHardcoreMessage = "PNACH cheat import is blocked while RetroAchievements Hardcore Mode is enabled."

    private init() {}

    @discardableResult
    func handleURL(_ url: URL) -> [ImportedGame] {
        handleURLs([url], preferredDestination: .automatic)
    }

    @discardableResult
    func handleURLs(
        _ urls: [URL],
        preferredDestination: ImportDestination = .automatic,
        allowReplacingExistingFiles: Bool = true
    ) -> [ImportedGame] {
        importURLs(
            urls,
            preferredDestination: preferredDestination,
            allowReplacingExistingFiles: allowReplacingExistingFiles
        )
    }

    @discardableResult
    func importURLs(
        _ urls: [URL],
        preferredDestination: ImportDestination = .automatic,
        allowReplacingExistingFiles: Bool = true
    ) -> [ImportedGame] {
        NSLog("[ARMSX2 iOS Import] importing %d file(s), destination=%@", urls.count, String(describing: preferredDestination))

        var imported: [String] = []
        var importedGames: [ImportedGame] = []
        var rejected: [String] = []
        var failed: [String] = []

        for url in urls {
            switch importFile(
                url,
                preferredDestination: preferredDestination,
                allowReplacingExistingFiles: allowReplacingExistingFiles
            ) {
            case .success(let message, let importedGame):
                imported.append(message)
                if let importedGame {
                    importedGames.append(importedGame)
                }
            case .unsupported(let message):
                rejected.append(message)
            case .failure(let message):
                failed.append(message)
            }
        }

        var lines: [String] = []
        if !imported.isEmpty {
            lines.append(imported.count == 1 ? imported[0] : "Imported \(imported.count) files.")
        }
        if !rejected.isEmpty {
            lines.append(rejected.joined(separator: "\n"))
        }
        if !failed.isEmpty {
            lines.append(failed.joined(separator: "\n"))
        }

        presentImportResult(lines.isEmpty ? "No files imported." : lines.joined(separator: "\n"))
        return importedGames
    }

    func existingFileNames(for urls: [URL], preferredDestination: ImportDestination) -> [String] {
        let directoryName: String
        let supportedExtensions: Set<String>
        switch preferredDestination {
        case .game:
            directoryName = "iso"
            supportedExtensions = Self.gameExtensions
        case .bios:
            directoryName = "bios"
            supportedExtensions = Self.biosExtensions
        default:
            return []
        }

        let docsPath = NSSearchPathForDirectoriesInDomains(.documentDirectory, .userDomainMask, true).first!
        let destinationDirectory = (docsPath as NSString).appendingPathComponent(directoryName)
        var existingFileNames: [String] = []
        var seenFileNames = Set<String>()

        for url in urls where supportedExtensions.contains(url.pathExtension.lowercased()) {
            let fileName = url.lastPathComponent
            let destinationPath = (destinationDirectory as NSString).appendingPathComponent(fileName)
            if FileManager.default.fileExists(atPath: destinationPath),
               seenFileNames.insert(fileName).inserted {
                existingFileNames.append(fileName)
            }
        }
        return existingFileNames
    }

    static func replacementConfirmationMessage(for fileNames: [String]) -> String {
        let visibleFileNames = fileNames.prefix(3)
        var lines = [
            "Some selected files already exist. Replacing them will overwrite the current copies.",
            "",
            fileNames.count == 1 ? "Existing file:" : "Existing files:"
        ]
        lines.append(contentsOf: visibleFileNames)
        if fileNames.count > visibleFileNames.count {
            lines.append("...and \(fileNames.count - visibleFileNames.count) more.")
        }
        return lines.joined(separator: "\n")
    }

    func presentImportResult(_ message: String) {
        lastImportMessage = message
        showImportAlert = true
    }

    private enum ImportResult {
        case success(String, importedGame: ImportedGame? = nil)
        case unsupported(message: String)
        case failure(String)
    }

    private func importFile(
        _ url: URL,
        preferredDestination: ImportDestination,
        allowReplacingExistingFiles: Bool
    ) -> ImportResult {
        let accessing = url.startAccessingSecurityScopedResource()
        defer { if accessing { url.stopAccessingSecurityScopedResource() } }

        let ext = url.pathExtension.lowercased()
        let fileName = url.lastPathComponent
        NSLog("[ARMSX2 iOS Import] candidate: %@ ext=%@ securityScoped=%d",
              fileName, ext.isEmpty ? "(none)" : ext, accessing ? 1 : 0)

        if preferredDestination == .pnachCheat || (preferredDestination == .automatic && Self.pnachExtensions.contains(ext)) {
            return importPNACHFile(url, destinationPath: ARMSX2Bridge.pnachPathForCurrentGame(asCheat: true), asCheat: true)
        }

        let docsPath = NSSearchPathForDirectoriesInDomains(.documentDirectory, .userDomainMask, true).first!

        // Determine destination
        let destDir: String
        let category: String

        if preferredDestination == .game {
            guard Self.gameExtensions.contains(ext) else {
                NSLog("[ARMSX2 iOS Import] unsupported game file: %@", fileName)
                return .unsupported(message: Self.unsupportedGameImportMessage(for: fileName))
            }
            destDir = (docsPath as NSString).appendingPathComponent("iso")
            category = "Game"
        } else if preferredDestination == .bios {
            guard Self.biosExtensions.contains(ext) else {
                NSLog("[ARMSX2 iOS Import] unsupported BIOS file: %@", fileName)
                return .unsupported(message: Self.unsupportedBIOSImportMessage(for: fileName))
            }

            let attrs = try? FileManager.default.attributesOfItem(atPath: url.path)
            let size = attrs?[.size] as? UInt64 ?? 0
            if size > 0 && size > Self.biosSizeThreshold {
                NSLog("[ARMSX2 iOS Import] rejecting oversized BIOS candidate: %@ size=%llu", fileName, size)
                return .failure(Self.oversizedBIOSImportMessage(for: fileName))
            }

            destDir = (docsPath as NSString).appendingPathComponent("bios")
            category = "BIOS"
        } else if Self.gameExtensions.subtracting(Self.biosExtensions).contains(ext) {
            destDir = (docsPath as NSString).appendingPathComponent("iso")
            category = "Game"
        } else if Self.biosExtensions.contains(ext) {
            // Check file size to distinguish BIOS (.bin ~4MB) from game (.bin ~700MB)
            let attrs = try? FileManager.default.attributesOfItem(atPath: url.path)
            let size = attrs?[.size] as? UInt64 ?? 0
            if ext == "bin" && size > Self.biosSizeThreshold {
                destDir = (docsPath as NSString).appendingPathComponent("iso")
                category = "Game"
            } else {
                destDir = (docsPath as NSString).appendingPathComponent("bios")
                category = "BIOS"
            }
        } else {
            NSLog("[ARMSX2 iOS Import] unsupported file: %@", fileName)
            return .unsupported(message: Self.unsupportedAutomaticImportMessage(for: fileName))
        }

        // Create directory if needed
        try? FileManager.default.createDirectory(atPath: destDir, withIntermediateDirectories: true)

        let destPath = (destDir as NSString).appendingPathComponent(fileName)

        // Copy file
        do {
            if FileManager.default.fileExists(atPath: destPath) {
                guard allowReplacingExistingFiles else {
                    return .failure("\(fileName) already exists. Import it again and choose Replace to overwrite the current copy.")
                }
                try FileManager.default.removeItem(atPath: destPath)
            }
            try ImportFileCopier.copy(from: url, to: URL(fileURLWithPath: destPath))
            NSLog("[ARMSX2 iOS Import] %@ imported: %@ -> %@", category, fileName, destPath)
            let importedGame = category == "Game" ? ImportedGame(name: fileName, fileURL: URL(fileURLWithPath: destPath)) : nil
            return .success("\(category) imported: \(fileName)", importedGame: importedGame)
        } catch {
            NSLog("[ARMSX2 iOS Import] failed: %@ -> %@ error=%@", fileName, destPath, error.localizedDescription)
            return .failure("\(fileName): \(error.localizedDescription)")
        }
    }

    private func importPNACHFile(_ url: URL, destinationPath: String?, asCheat: Bool) -> ImportResult {
        let fileName = url.lastPathComponent
        if !Self.pnachExtensions.contains(url.pathExtension.lowercased()) {
            NSLog("[ARMSX2 iOS Import] PNACH file has non-standard extension, attempting text import: %@", fileName)
        }

        guard let name = ARMSX2Bridge.currentGameISOName(), !name.isEmpty else {
            return .failure(Self.pnachImportNeedsGameMessage)
        }

        let accessing = url.startAccessingSecurityScopedResource()
        let data = try? Data(contentsOf: url)
        if accessing { url.stopAccessingSecurityScopedResource() }

        guard let data, let text = (String(data: data, encoding: .utf8) ?? String(data: data, encoding: .ascii)) else {
            return .failure(Self.unreadablePNACHImportMessage(for: fileName))
        }

        // Wrap headerless (legacy) patches so the import stays a distinct, manageable entry
        // instead of being absorbed into an existing section (matches the manager import path).
        let installText = PnachParser.wrappingLegacyPatches(text, header: PatchStore.importLabel(for: url))
        let outcome = PatchStore.shared.writePatch(text: installText, forISO: name, asCheat: asCheat, autoEnable: true, source: .local)
        switch outcome {
        case .success:
            NSLog("[ARMSX2 iOS Import] PNACH imported: %@", fileName)
            return .success("PNACH imported for \(name)")
        case .invalid(let reason):
            return .failure(Self.failedPNACHImportMessage(for: fileName, errorDescription: reason))
        case .blockedByHardcore:
            return .failure(Self.pnachCheatBlockedByHardcoreMessage)
        case .noIdentity:
            return .failure(Self.pnachImportNeedsGameMessage)
        case .writeFailed(let reason):
            return .failure(Self.failedPNACHImportMessage(for: fileName, errorDescription: reason))
        }
    }

    static func isUserCancelledPickerError(_ error: Error) -> Bool {
        (error as NSError).code == NSUserCancelledError
    }

    static func failedGamePickerMessage(errorDescription: String) -> String {
        "Game import failed. Try importing a supported PS2 game image.\n\(errorDescription)"
    }

    static func failedBIOSPickerMessage(errorDescription: String) -> String {
        "BIOS import failed. Try importing a PS2 BIOS dump.\n\(errorDescription)"
    }

    static func failedPNACHPickerMessage(errorDescription: String) -> String {
        "PNACH import failed. Try importing a text patch for the selected game.\n\(errorDescription)"
    }

    private static func contentTypes(for extensions: [String]) -> [UTType] {
        extensions.map { ext in
            UTType(filenameExtension: ext) ?? UTType(importedAs: "com.armsx2.\(ext)", conformingTo: .data)
        }
    }

    private static func broaderContentTypes(for extensions: [String]) -> [UTType] {
        Array(Set([.item, .data, .content] + contentTypes(for: extensions)))
    }

    private static func unsupportedGameImportMessage(for fileName: String) -> String {
        "This does not look like a supported game image: \(fileName). Try importing a PS2 game file such as \(formatList(gameExtensionList))."
    }

    private static func unsupportedBIOSImportMessage(for fileName: String) -> String {
        "This does not look like a PS2 BIOS file: \(fileName). Try importing a BIOS dump such as \(formatList(biosExtensionList))."
    }

    private static func oversizedBIOSImportMessage(for fileName: String) -> String {
        "This file is too large to be a normal PS2 BIOS dump: \(fileName). Try importing a BIOS dump between 1 MB and 50 MB, usually \(formatList(biosExtensionList))."
    }

    private static func unsupportedAutomaticImportMessage(for fileName: String) -> String {
        "ARMSX2 could not import \(fileName). Try importing it from the matching Games, BIOS, or PNACH patch option."
    }

    private static func unreadablePNACHImportMessage(for fileName: String) -> String {
        "\(fileName) is not a readable PNACH patch. PNACH patches need to be UTF-8 or ASCII text."
    }

    private static func failedPNACHImportMessage(for fileName: String, errorDescription: String) -> String {
        "PNACH import failed for \(fileName). Check that this is a text patch for the selected game.\n\(errorDescription)"
    }

    private static func formatList(_ extensions: [String]) -> String {
        let formats = extensions.map { ".\($0)" }
        guard let last = formats.last else {
            return ""
        }
        if formats.count == 1 {
            return last
        }
        return "\(formats.dropLast().joined(separator: ", ")), or \(last)"
    }
}
