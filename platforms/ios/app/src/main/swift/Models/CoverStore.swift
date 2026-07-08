// CoverStore.swift - Local game cover lookup/import for the iOS game library
// SPDX-License-Identifier: GPL-3.0+

import Foundation
import ImageIO
import SwiftUI
import UniformTypeIdentifiers

struct CoverGameInfo: Sendable {
    let name: String
    let fileURL: URL?
    let metadata: [String: String]
    let hasCover: Bool
}

struct CoverDownloadSummary: Sendable {
    let downloaded: Int
    let skippedExisting: Int
    let failed: Int
}

@MainActor
@Observable
final class CoverStore: @unchecked Sendable {
    static let shared = CoverStore()

    static let defaultCoverURLTemplate = "https://raw.githubusercontent.com/xlenore/ps2-covers/main/covers/default/${serial}.jpg"

    static let imageExtensions: [String] = ["jpg", "jpeg", "png", "webp", "heic", "heif"]
    static let coverContentTypes: [UTType] = {
        var types: [UTType] = [.item, .data, .content, .image]
        for ext in imageExtensions {
            if let type = UTType(filenameExtension: ext), !types.contains(type) {
                types.append(type)
            }
        }
        return types
    }()

    private let fileManager = FileManager.default

    var lastCoverMessage: String?
    var showCoverAlert = false
    var coverURLTemplate: String {
        get {
            let stored = UserDefaults.standard.string(forKey: "ARMSX2iOSCoverURLTemplate")
            return stored?.isEmpty == false ? stored! : Self.defaultCoverURLTemplate
        }
        set {
            UserDefaults.standard.set(newValue.trimmingCharacters(in: .whitespacesAndNewlines), forKey: "ARMSX2iOSCoverURLTemplate")
        }
    }
    var isDownloadingCovers = false

    private init() {}

    var primaryCoverDirectory: URL {
        let docs = URL(fileURLWithPath: ARMSX2Bridge.documentsDirectory(), isDirectory: true)
        let dir = docs.appendingPathComponent("armsx2_covers", isDirectory: true)
        try? fileManager.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir
    }

    func coverURL(forGameName gameName: String, gamePath: URL?) -> URL? {
        let candidates = coverBaseCandidates(forGameName: gameName, metadata: [:])
        for dir in coverSearchDirectories(gamePath: gamePath) {
            if let url = findCover(in: dir, matching: candidates) {
                return url
            }
        }
        return nil
    }

    func coverURL(forGameName gameName: String, gamePath: URL?, metadata: [String: String]) -> URL? {
        let candidates = coverBaseCandidates(forGameName: gameName, metadata: metadata)
        for dir in coverSearchDirectories(gamePath: gamePath) {
            if let url = findCover(in: dir, matching: candidates) {
                return url
            }
        }
        return nil
    }

    func downloadMissingCovers(for games: [CoverGameInfo], showResult: Bool = true) async -> CoverDownloadSummary {
        let template = coverURLTemplate.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !template.isEmpty else {
            if showResult {
                lastCoverMessage = "Set a cover URL template first."
                showCoverAlert = true
            } else {
                NSLog("[ARMSX2 iOS Covers] auto-download skipped: cover URL template is empty")
            }
            return CoverDownloadSummary(downloaded: 0, skippedExisting: 0, failed: games.count)
        }

        guard !isDownloadingCovers else {
            if showResult {
                lastCoverMessage = "Cover download already in progress."
                showCoverAlert = true
            } else {
                NSLog("[ARMSX2 iOS Covers] auto-download skipped: cover download already in progress")
            }
            return CoverDownloadSummary(downloaded: 0, skippedExisting: 0, failed: 0)
        }

        isDownloadingCovers = true
        defer { isDownloadingCovers = false }

        var downloaded = 0
        var skipped = 0
        var failed = 0
        var attempted = Set<String>()

        for game in games {
            if game.hasCover || coverURL(forGameName: game.name, gamePath: game.fileURL, metadata: game.metadata) != nil {
                skipped += 1
                continue
            }

            let candidates = buildCoverCandidateURLs(forGameName: game.name, metadata: game.metadata, template: template)
            guard !candidates.isEmpty else {
                failed += 1
                continue
            }

            var didDownload = false
            for url in candidates where attempted.insert(url.absoluteString).inserted {
                if await downloadCover(from: url, forGameName: game.name, metadata: game.metadata) {
                    downloaded += 1
                    didDownload = true
                    break
                }
            }
            if !didDownload {
                failed += 1
            }
        }

        if showResult {
            let summary = "Downloaded \(downloaded) cover(s). Skipped \(skipped) existing. Failed \(failed)."
            lastCoverMessage = failed > 0
                ? "\(summary)\nCheck your connection and Cover Source, or choose a local cover."
                : summary
            showCoverAlert = true
        } else {
            NSLog("[ARMSX2 iOS Covers] auto-download finished downloaded=%d skipped=%d failed=%d", downloaded, skipped, failed)
        }
        return CoverDownloadSummary(downloaded: downloaded, skippedExisting: skipped, failed: failed)
    }

    @discardableResult
    func importCoverURLs(_ urls: [URL], forGameNamed gameName: String? = nil) -> String {
        var imported: [String] = []
        var rejected: [String] = []
        var failed: [String] = []

        if let gameName, urls.count == 1 {
            removeManagedCovers(forGameNamed: gameName)
        }

        for (index, sourceURL) in urls.enumerated() {
            let accessing = sourceURL.startAccessingSecurityScopedResource()
            defer { if accessing { sourceURL.stopAccessingSecurityScopedResource() } }

            let ext = normalizedImageExtension(for: sourceURL)
            guard Self.imageExtensions.contains(ext) else {
                rejected.append(sourceURL.lastPathComponent)
                continue
            }

            let baseName: String
            if let gameName, urls.count == 1 {
                baseName = preferredManagedCoverBaseName(forGameName: gameName)
            } else {
                let sourceBase = sourceURL.deletingPathExtension().lastPathComponent
                let fallback = sourceBase.isEmpty ? "cover_\(index + 1)" : sourceBase
                baseName = sanitizedCoverComponent(fallback)
            }

            let destination = primaryCoverDirectory.appendingPathComponent(baseName).appendingPathExtension(ext)
            do {
                try replaceCover(at: destination) {
                    try fileManager.copyItem(at: sourceURL, to: destination)
                }
                imported.append(sourceURL.lastPathComponent)
                NSLog("[ARMSX2 iOS Covers] imported %@ -> %@", sourceURL.lastPathComponent, destination.path)
            } catch {
                failed.append("\(sourceURL.lastPathComponent): \(error.localizedDescription)")
                NSLog("[ARMSX2 iOS Covers] import failed %@ -> %@ error=%@", sourceURL.lastPathComponent, destination.path, error.localizedDescription)
            }
        }

        var lines: [String] = []
        if !imported.isEmpty {
            let prefix = gameName == nil ? "Imported cover" : "Assigned cover"
            lines.append(imported.count == 1 ? "\(prefix): \(imported[0])" : "\(prefix)s: \(imported.count)")
        }
        if !rejected.isEmpty {
            lines.append("Unsupported image: \(rejected.joined(separator: ", "))")
        }
        if !failed.isEmpty {
            lines.append(failed.joined(separator: "\n"))
        }

        let message = lines.isEmpty ? "No covers imported." : lines.joined(separator: "\n")
        lastCoverMessage = message
        showCoverAlert = true
        return message
    }

    @discardableResult
    func importCoverData(_ data: Data, forGameNamed gameName: String) -> String {
        guard let ext = imageExtension(for: data) else {
            let message = "The selected photo is not a supported image."
            lastCoverMessage = message
            showCoverAlert = true
            return message
        }

        removeManagedCovers(forGameNamed: gameName)
        let baseName = preferredManagedCoverBaseName(forGameName: gameName)
        let destination = primaryCoverDirectory.appendingPathComponent(baseName).appendingPathExtension(ext)
        let message: String
        do {
            try replaceCover(at: destination) {
                try data.write(to: destination, options: .atomic)
            }
            message = "Assigned cover from Photos."
            NSLog("[ARMSX2 iOS Covers] imported photo -> %@", destination.path)
        } catch {
            message = "Cover import failed: \(error.localizedDescription)"
            NSLog("[ARMSX2 iOS Covers] photo import failed -> %@ error=%@", destination.path, error.localizedDescription)
        }

        lastCoverMessage = message
        showCoverAlert = true
        return message
    }

    @discardableResult
    func removeManagedCovers(forGameNamed gameName: String) -> Int {
        let candidates = Set(coverBaseCandidates(forGameName: gameName, metadata: [:]).map { $0.lowercased() })
        var removed = 0

        for dir in managedCoverDirectories() {
            guard let files = try? fileManager.contentsOfDirectory(at: dir, includingPropertiesForKeys: [.isRegularFileKey], options: [.skipsHiddenFiles]) else {
                continue
            }
            for file in files {
                let ext = file.pathExtension.lowercased()
                guard Self.imageExtensions.contains(ext) else { continue }
                let base = file.deletingPathExtension().lastPathComponent.lowercased()
                guard candidates.contains(base) else { continue }
                do {
                    try fileManager.removeItem(at: file)
                    removed += 1
                    NSLog("[ARMSX2 iOS Covers] removed %@", file.path)
                } catch {
                    NSLog("[ARMSX2 iOS Covers] remove failed %@ error=%@", file.path, error.localizedDescription)
                }
            }
        }

        if removed > 0 {
            lastCoverMessage = "Removed cover for \(displayName(forGameName: gameName))."
        } else {
            lastCoverMessage = "No managed cover found for \(displayName(forGameName: gameName))."
        }
        showCoverAlert = true
        return removed
    }

    func displayName(forGameName gameName: String) -> String {
        URL(fileURLWithPath: gameName).deletingPathExtension().lastPathComponent
    }

    func buildCoverCandidateURLs(forGameName gameName: String, metadata: [String: String], template: String? = nil) -> [URL] {
        let template = (template ?? coverURLTemplate).trimmingCharacters(in: .whitespacesAndNewlines)
        guard !template.isEmpty else { return [] }

        let fileBase = displayName(forGameName: gameName)
        let title = metadata["title"]?.trimmingCharacters(in: .whitespacesAndNewlines)
        let serial = metadata["serial"]?.trimmingCharacters(in: .whitespacesAndNewlines)
        var rawURLs: [String] = []

        if template.contains("${serial}") {
            var serialValues: [String] = []
            if let serial, !serial.isEmpty {
                serialValues.append(serial)
            }
            serialValues.append(contentsOf: serialCandidates(from: fileBase))
            serialValues.append(contentsOf: titleVariants(from: fileBase))
            for value in uniqueStrings(serialValues) {
                rawURLs.append(fillTemplate(template, serial: value, title: "", fileTitle: ""))
            }
        }

        if template.contains("${title}") {
            var titleValues = titleVariants(from: title?.isEmpty == false ? title! : fileBase)
            if title != fileBase {
                titleValues.append(contentsOf: titleVariants(from: fileBase))
            }
            for value in uniqueStrings(titleValues) {
                rawURLs.append(fillTemplate(template, serial: "", title: value, fileTitle: ""))
            }
        }

        if template.contains("${filetitle}") {
            for value in uniqueStrings(titleVariants(from: fileBase)) {
                rawURLs.append(fillTemplate(template, serial: "", title: "", fileTitle: value))
            }
        }

        return uniqueStrings(rawURLs)
            .filter { !$0.contains("${") }
            .compactMap(URL.init(string:))
    }

    private func managedCoverDirectories() -> [URL] {
        let docs = URL(fileURLWithPath: ARMSX2Bridge.documentsDirectory(), isDirectory: true)
        let primary = primaryCoverDirectory
        let legacy = docs.appendingPathComponent("covers", isDirectory: true)
        try? fileManager.createDirectory(at: legacy, withIntermediateDirectories: true)
        return uniqueURLs([primary, legacy])
    }

    private func coverSearchDirectories(gamePath: URL?) -> [URL] {
        let docs = URL(fileURLWithPath: ARMSX2Bridge.documentsDirectory(), isDirectory: true)
        let iso = URL(fileURLWithPath: ARMSX2Bridge.isoDirectory(), isDirectory: true)
        var dirs = managedCoverDirectories()
        if let gamePath {
            dirs.append(gamePath.deletingLastPathComponent())
        }
        dirs.append(iso)
        dirs.append(docs)
        return uniqueURLs(dirs)
    }

    private func findCover(in directory: URL, matching candidates: [String]) -> URL? {
        guard let files = try? fileManager.contentsOfDirectory(at: directory, includingPropertiesForKeys: [.isRegularFileKey], options: [.skipsHiddenFiles]) else {
            return nil
        }

        var lookup: [String: URL] = [:]
        for file in files {
            let ext = file.pathExtension.lowercased()
            guard Self.imageExtensions.contains(ext) else { continue }
            if let values = try? file.resourceValues(forKeys: [.isRegularFileKey]), values.isRegularFile == false {
                continue
            }
            lookup[file.lastPathComponent.lowercased()] = file
        }

        for candidate in candidates {
            let lower = candidate.lowercased()
            for ext in Self.imageExtensions {
                if let file = lookup["\(lower).\(ext)"] {
                    return file
                }
            }
        }

        return nil
    }

    private func coverBaseCandidates(forGameName gameName: String, metadata: [String: String]) -> [String] {
        let fileName = URL(fileURLWithPath: gameName).lastPathComponent
        let stem = URL(fileURLWithPath: fileName).deletingPathExtension().lastPathComponent
        let nestedStem = nestedDiscStem(from: stem)
        var raw: [String] = [stem, fileName]

        if let serial = metadata["serial"], !serial.isEmpty {
            raw.append(serial)
        }
        if let title = metadata["title"], !title.isEmpty {
            raw.append(title)
        }
        if let nestedStem, nestedStem != stem {
            raw.append(nestedStem)
        }
        raw.append(contentsOf: titleVariants(from: stem))
        if let nestedStem {
            raw.append(contentsOf: titleVariants(from: nestedStem))
        }
        raw.append(contentsOf: serialCandidates(from: stem))
        if let nestedStem {
            raw.append(contentsOf: serialCandidates(from: nestedStem))
        }

        var result: [String] = []
        var seen = Set<String>()
        for item in raw {
            for candidate in [item, sanitizedCoverComponent(item)] {
                let trimmed = candidate.trimmingCharacters(in: .whitespacesAndNewlines)
                guard !trimmed.isEmpty else { continue }
                let key = trimmed.lowercased()
                if seen.insert(key).inserted {
                    result.append(trimmed)
                }
            }
        }
        return result
    }

    private func downloadCover(from remoteURL: URL, forGameName gameName: String, metadata: [String: String]) async -> Bool {
        do {
            var request = URLRequest(url: remoteURL)
            request.timeoutInterval = 8
            request.cachePolicy = .reloadIgnoringLocalCacheData

            let (data, response) = try await URLSession.shared.data(for: request)
            guard let http = response as? HTTPURLResponse, http.statusCode == 200, !data.isEmpty else {
                NSLog("[ARMSX2 iOS Covers] HTTP miss %@", remoteURL.absoluteString)
                return false
            }

            let ext = imageExtension(from: remoteURL, response: http)
            let baseName = preferredDownloadedCoverBaseName(forGameName: gameName, metadata: metadata)
            let target = primaryCoverDirectory.appendingPathComponent(baseName).appendingPathExtension(ext)
            let temp = primaryCoverDirectory.appendingPathComponent("\(baseName).download").appendingPathExtension(ext)

            if fileManager.fileExists(atPath: target.path) {
                try fileManager.removeItem(at: target)
            }
            if fileManager.fileExists(atPath: temp.path) {
                try fileManager.removeItem(at: temp)
            }
            try data.write(to: temp, options: .atomic)
            try fileManager.moveItem(at: temp, to: target)
            NSLog("[ARMSX2 iOS Covers] downloaded %@ -> %@", remoteURL.absoluteString, target.path)
            return true
        } catch {
            NSLog("[ARMSX2 iOS Covers] download failed %@ error=%@", remoteURL.absoluteString, error.localizedDescription)
            return false
        }
    }

    private func preferredDownloadedCoverBaseName(forGameName gameName: String, metadata: [String: String]) -> String {
        if let serial = metadata["serial"], !serial.isEmpty {
            let sanitized = sanitizedCoverComponent(serial)
            if !sanitized.isEmpty {
                return sanitized
            }
        }
        if let title = metadata["title"], !title.isEmpty {
            let sanitized = sanitizedCoverComponent(title)
            if !sanitized.isEmpty {
                return sanitized
            }
        }
        return preferredManagedCoverBaseName(forGameName: gameName)
    }

    private func fillTemplate(_ template: String, serial: String, title: String, fileTitle: String) -> String {
        template
            .replacingOccurrences(of: "${serial}", with: urlComponent(serial))
            .replacingOccurrences(of: "${title}", with: urlComponent(title))
            .replacingOccurrences(of: "${filetitle}", with: urlComponent(fileTitle))
    }

    private func urlComponent(_ value: String) -> String {
        value.addingPercentEncoding(withAllowedCharacters: .urlPathAllowed) ?? value
    }

    private func imageExtension(from url: URL, response: HTTPURLResponse) -> String {
        if let contentType = response.value(forHTTPHeaderField: "Content-Type")?.lowercased() {
            if contentType.contains("png") { return "png" }
            if contentType.contains("webp") { return "webp" }
            if contentType.contains("heic") { return "heic" }
            if contentType.contains("heif") { return "heif" }
            if contentType.contains("jpeg") || contentType.contains("jpg") { return "jpg" }
        }

        let ext = url.pathExtension.lowercased()
        if Self.imageExtensions.contains(ext) {
            return ext == "jpeg" ? "jpg" : ext
        }
        return "jpg"
    }

    private func imageExtension(for data: Data) -> String? {
        guard let imageSource = CGImageSourceCreateWithData(data as CFData, nil),
              let typeIdentifier = CGImageSourceGetType(imageSource),
              let ext = UTType(typeIdentifier as String)?.preferredFilenameExtension?.lowercased() else {
            return nil
        }

        let normalized = ext == "jpeg" ? "jpg" : ext
        return Self.imageExtensions.contains(normalized) ? normalized : nil
    }

    private func replaceCover(at destination: URL, write: () throws -> Void) throws {
        if fileManager.fileExists(atPath: destination.path) {
            try fileManager.removeItem(at: destination)
        }
        try write()
    }

    private func preferredManagedCoverBaseName(forGameName gameName: String) -> String {
        let stem = URL(fileURLWithPath: gameName).deletingPathExtension().lastPathComponent
        let sanitized = sanitizedCoverComponent(stem)
        return sanitized.isEmpty ? "cover" : sanitized
    }

    private func nestedDiscStem(from stem: String) -> String? {
        let url = URL(fileURLWithPath: stem)
        let nestedExt = url.pathExtension.lowercased()
        let discExtensions: Set<String> = ["iso", "img", "bin", "cso", "zso", "gz"]
        guard discExtensions.contains(nestedExt) else { return nil }
        let nestedStem = url.deletingPathExtension().lastPathComponent
        return nestedStem.isEmpty ? nil : nestedStem
    }

    private func titleVariants(from base: String) -> [String] {
        let normalized = base.trimmingCharacters(in: .whitespacesAndNewlines)
        let spaceNormalized = normalized.replacingOccurrences(of: "_", with: " ")
            .replacingOccurrences(of: "\\s+", with: " ", options: .regularExpression)
            .trimmingCharacters(in: .whitespacesAndNewlines)
        let stripped = strippingBracketedTags(from: spaceNormalized)
        var variants: [String] = [normalized, spaceNormalized, stripped]

        let colonToDash = spaceNormalized.replacingOccurrences(of: ":", with: " - ")
            .replacingOccurrences(of: "\\s+", with: " ", options: .regularExpression)
            .trimmingCharacters(in: .whitespacesAndNewlines)
        variants.append(colonToDash)

        let dashToColon = spaceNormalized.replacingOccurrences(of: " - ", with: ": ")
            .replacingOccurrences(of: "\\s+", with: " ", options: .regularExpression)
            .trimmingCharacters(in: .whitespacesAndNewlines)
        variants.append(dashToColon)

        let strippedDashToColon = stripped.replacingOccurrences(of: " - ", with: ": ")
            .replacingOccurrences(of: "\\s+", with: " ", options: .regularExpression)
            .trimmingCharacters(in: .whitespacesAndNewlines)
        variants.append(strippedDashToColon)

        return uniqueStrings(variants)
    }

    private func strippingBracketedTags(from base: String) -> String {
        var value = base
        for pattern in ["\\[[^\\]]*\\]", "\\([^\\)]*\\)", "\\{[^\\}]*\\}"] {
            value = value.replacingOccurrences(of: pattern, with: " ", options: .regularExpression)
        }
        return value.replacingOccurrences(of: "\\s+", with: " ", options: .regularExpression)
            .trimmingCharacters(in: .whitespacesAndNewlines)
    }

    private func serialCandidates(from base: String) -> [String] {
        let pattern = "^([A-Za-z]{4})[_-]?([0-9]{3})[._-]?([0-9]{2})"
        guard let regex = try? NSRegularExpression(pattern: pattern) else { return [] }
        let range = NSRange(base.startIndex..<base.endIndex, in: base)
        guard let match = regex.firstMatch(in: base, range: range),
              match.numberOfRanges >= 4,
              let prefixRange = Range(match.range(at: 1), in: base),
              let firstRange = Range(match.range(at: 2), in: base),
              let secondRange = Range(match.range(at: 3), in: base) else {
            return []
        }
        let prefix = String(base[prefixRange]).uppercased()
        let first = String(base[firstRange])
        let second = String(base[secondRange])
        return ["\(prefix)-\(first)\(second)", "\(prefix)_\(first).\(second)"]
    }

    private func sanitizedCoverComponent(_ input: String) -> String {
        var value = input.trimmingCharacters(in: .whitespacesAndNewlines)
        value = value.replacingOccurrences(of: "[\\\\/:*?\"<>|]", with: " ", options: .regularExpression)
        value = value.replacingOccurrences(of: "[^A-Za-z0-9._-]", with: "_", options: .regularExpression)
        value = value.replacingOccurrences(of: "_+", with: "_", options: .regularExpression)
        value = value.replacingOccurrences(of: "^_+|_+$", with: "", options: .regularExpression)
        return value
    }

    private func normalizedImageExtension(for url: URL) -> String {
        let ext = url.pathExtension.lowercased()
        return ext == "jpeg" ? "jpg" : ext
    }

    private func uniqueURLs(_ urls: [URL]) -> [URL] {
        var seen = Set<String>()
        var result: [URL] = []
        for url in urls {
            let key = url.standardizedFileURL.path
            if seen.insert(key).inserted {
                result.append(url)
            }
        }
        return result
    }

    private func uniqueStrings(_ values: [String]) -> [String] {
        var seen = Set<String>()
        var result: [String] = []
        for value in values {
            let trimmed = value.trimmingCharacters(in: .whitespacesAndNewlines)
            guard !trimmed.isEmpty else { continue }
            let key = trimmed.lowercased()
            if seen.insert(key).inserted {
                result.append(trimmed)
            }
        }
        return result
    }
}
