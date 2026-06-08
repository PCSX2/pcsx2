// ARMSX2Mac native macOS support layer
// SPDX-License-Identifier: GPL-3.0+

import AppKit
import CoreGraphics
import Foundation
import GameController
import SwiftUI
import UniformTypeIdentifiers

enum MacPaths {
    static let appDisplayName = "ARMSX2-MacOS 2.0"
    static let dataDirectoryName = "ARMSX2-MacOS 2.0"
    static let legacyDataDirectoryName = "iPSX2"

    static var dataRoot: URL {
        if let override = UserDefaults.standard.string(forKey: "ARMSX2MacDataRoot"), !override.isEmpty {
            return URL(fileURLWithPath: override, isDirectory: true)
        }

        let documents = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first!
        return documents.appendingPathComponent(dataDirectoryName, isDirectory: true)
    }

    static var settingsFile: URL {
        directory("inis").appendingPathComponent("PCSX2.ini")
    }

    static var secretsFile: URL {
        directory("inis").appendingPathComponent("secrets.ini")
    }

    static var emulatorExecutable: URL? {
        if let bundled = bundledEmulatorExecutable {
            return bundled
        }

        if let stored = UserDefaults.standard.string(forKey: "ARMSX2MacEmulatorExecutable"), !stored.isEmpty {
            let storedURL = URL(fileURLWithPath: stored)
            if FileManager.default.isExecutableFile(atPath: storedURL.path) {
                return storedURL
            }
        }

        let roots = executableSearchRoots()
        let relativeCandidates = [
            "build/pcsx2-qt/pcsx2-qt.app/Contents/MacOS/pcsx2-qt",
            "build/pcsx2-qt/ARMSX2.app/Contents/MacOS/ARMSX2",
            "build/bin/pcsx2-qt",
            "build/pcsx2-qt/pcsx2-qt",
            "pcsx2-qt.app/Contents/MacOS/pcsx2-qt",
            "ARMSX2.app/Contents/MacOS/ARMSX2",
            "Contents/Helpers/ARMSX2.app/Contents/MacOS/ARMSX2",
            "Contents/Helpers/pcsx2-qt.app/Contents/MacOS/pcsx2-qt",
        ]
        let candidates = roots.flatMap { root in
            relativeCandidates.map { root.appendingPathComponent($0) }
        }
        return candidates.first { FileManager.default.isExecutableFile(atPath: $0.path) }
    }

    static func directory(_ name: String) -> URL {
        let url = dataRoot.appendingPathComponent(name, isDirectory: true)
        try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }

    static func ensureLayout() {
        migrateLegacyDataRootIfNeeded()
        [
            "bios", "iso", "logs", "memcards", "savestates", "snaps", "cheats",
            "patches", "cache", "covers", "gamesettings", "textures",
            "inputprofiles", "videos", "inis", "resources",
        ].forEach { _ = directory($0) }
    }

    private static func executableSearchRoots() -> [URL] {
        let bundle = Bundle.main.bundleURL
        let current = URL(fileURLWithPath: FileManager.default.currentDirectoryPath, isDirectory: true)
        let home = URL(fileURLWithPath: NSHomeDirectory(), isDirectory: true)
        let likelyWorktree = bundle
            .deletingLastPathComponent()
            .deletingLastPathComponent()

        var roots = [current, likelyWorktree, likelyWorktree.deletingLastPathComponent(), home]
        roots.append(contentsOf: [
            URL(fileURLWithPath: "/Applications", isDirectory: true),
            URL(fileURLWithPath: "\(NSHomeDirectory())/Applications", isDirectory: true),
        ])
        return uniqueURLs(roots)
    }

    private static var bundledEmulatorExecutable: URL? {
        let bundle = Bundle.main.bundleURL
        let candidates = [
            bundle.appendingPathComponent("Contents/Helpers/ARMSX2.app/Contents/MacOS/ARMSX2"),
            bundle.appendingPathComponent("Contents/Helpers/pcsx2-qt.app/Contents/MacOS/pcsx2-qt"),
            bundle.appendingPathComponent("Contents/Helpers/ARMSX2"),
            bundle.appendingPathComponent("Contents/Helpers/pcsx2-qt"),
        ]
        return candidates.first { FileManager.default.isExecutableFile(atPath: $0.path) }
    }

    private static func migrateLegacyDataRootIfNeeded() {
        let fm = FileManager.default
        let documents = fm.urls(for: .documentDirectory, in: .userDomainMask).first!
        let legacy = documents.appendingPathComponent(legacyDataDirectoryName, isDirectory: true)
        let current = dataRoot
        guard fm.fileExists(atPath: legacy.path), !fm.fileExists(atPath: current.path) else { return }

        do {
            try fm.moveItem(at: legacy, to: current)
        } catch {
            try? fm.createDirectory(at: current, withIntermediateDirectories: true)
            if let children = try? fm.contentsOfDirectory(at: legacy, includingPropertiesForKeys: nil) {
                for child in children {
                    let destination = current.appendingPathComponent(child.lastPathComponent)
                    if !fm.fileExists(atPath: destination.path) {
                        try? fm.copyItem(at: child, to: destination)
                    }
                }
            }
        }
    }

    private static func uniqueURLs(_ urls: [URL]) -> [URL] {
        var seen = Set<String>()
        return urls.filter { seen.insert($0.standardizedFileURL.path).inserted }
    }
}

final class INIFile {
    private let url: URL
    private var values: [String: [String: [String]]] = [:]

    init(url: URL) {
        self.url = url
        reload()
    }

    func reload() {
        values.removeAll()

        guard let text = try? String(contentsOf: url, encoding: .utf8) else { return }
        var section = ""
        for rawLine in text.components(separatedBy: .newlines) {
            let line = rawLine.trimmingCharacters(in: .whitespacesAndNewlines)
            guard !line.isEmpty, !line.hasPrefix("#"), !line.hasPrefix(";") else { continue }

            if line.hasPrefix("[") && line.hasSuffix("]") {
                section = String(line.dropFirst().dropLast())
                if values[section] == nil {
                    values[section] = [:]
                }
                continue
            }

            guard let equals = line.firstIndex(of: "=") else { continue }
            let key = String(line[..<equals]).trimmingCharacters(in: .whitespaces)
            let value = String(line[line.index(after: equals)...]).trimmingCharacters(in: .whitespaces)
            values[section, default: [:]][key, default: []].append(value)
        }
    }

    func string(_ section: String, _ key: String, default defaultValue: String) -> String {
        values[section]?[key]?.last ?? defaultValue
    }

    func stringList(_ section: String, _ key: String) -> [String] {
        values[section]?[key] ?? []
    }

    func bool(_ section: String, _ key: String, default defaultValue: Bool) -> Bool {
        let value = string(section, key, default: defaultValue ? "true" : "false").lowercased()
        return ["1", "true", "yes", "on"].contains(value)
    }

    func int(_ section: String, _ key: String, default defaultValue: Int) -> Int {
        Int(string(section, key, default: "\(defaultValue)")) ?? defaultValue
    }

    func float(_ section: String, _ key: String, default defaultValue: Double) -> Double {
        Double(string(section, key, default: "\(defaultValue)")) ?? defaultValue
    }

    func set(_ section: String, _ key: String, _ value: String) {
        values[section, default: [:]][key] = [value]
        save()
    }

    func setList(_ section: String, _ key: String, _ items: [String]) {
        if items.isEmpty {
            values[section]?.removeValue(forKey: key)
        } else {
            values[section, default: [:]][key] = items
        }
        save()
    }

    func set(_ section: String, _ key: String, _ value: Bool) {
        set(section, key, value ? "true" : "false")
    }

    func set(_ section: String, _ key: String, _ value: Int) {
        set(section, key, "\(value)")
    }

    func set(_ section: String, _ key: String, _ value: Double) {
        set(section, key, String(format: "%.3f", value))
    }

    func delete(_ section: String, _ key: String) {
        values[section]?.removeValue(forKey: key)
        save()
    }

    func save() {
        var output: [String] = []
        for section in values.keys.sorted() {
            if !section.isEmpty {
                output.append("[\(section)]")
            }
            for key in values[section, default: [:]].keys.sorted() {
                for value in values[section]?[key] ?? [] {
                    output.append("\(key) = \(value)")
                }
            }
            output.append("")
        }
        try? FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? output.joined(separator: "\n").write(to: url, atomically: true, encoding: .utf8)
    }
}

struct GameEntry: Identifiable, Hashable {
    let id: String
    let name: String
    let url: URL
    let size: UInt64
    let source: String
    let serial: String?
    let region: String?
    let coverURL: URL?

    var displayName: String {
        Self.cleanedTitle(from: url.deletingPathExtension().lastPathComponent)
    }

    var sizeText: String {
        let gb = Double(size) / 1_073_741_824
        if gb >= 1 {
            return String(format: "%.1f GB", gb)
        }
        return String(format: "%.0f MB", Double(size) / 1_048_576)
    }

    static func cleanedTitle(from raw: String) -> String {
        var title = raw
        let patterns = [
            #"\s*\[[^\]]+\]"#,
            #"\s*\([A-Z]{2,}(?:[,\s][^)]+)?\)"#,
            #"\s*\((USA|Canada|Europe|Japan|Korea|Australia|Asia|Germany|France|China|En|Fr|De|Es|It|Ru|Rev\s*\d+|v\d+(?:\.\d+)?|Beta|Demo)[^)]*\)"#,
        ]
        for pattern in patterns {
            title = title.replacingOccurrences(of: pattern, with: "", options: [.regularExpression, .caseInsensitive])
        }
        return title.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty ? raw : title.trimmingCharacters(in: .whitespacesAndNewlines)
    }
}

@MainActor
final class GameLibrary: ObservableObject {
    @Published var games: [GameEntry] = []
    @Published var lastMessage: String?

    private let metadata = GameMetadataDatabase.shared
    private let gameExtensions = Set(["iso", "chd", "img", "bin", "cue", "mdf", "cso", "zso", "gz", "elf"])
    private let imageExtensions = Set(["jpg", "jpeg", "png", "webp", "heic", "heif"])
    private var autoCoverDownloadInProgress = false
    private var autoCoverAttemptedGameIDs = Set<GameEntry.ID>()

    init() {
        MacPaths.ensureLayout()
        refresh(autoDownloadCovers: false)
        scheduleAutoCoverDownload()
    }

    func refresh(autoDownloadCovers: Bool = true) {
        let roots = [MacPaths.directory("iso"), MacPaths.dataRoot] + externalGameFolders()
        var seen = Set<String>()
        var scanned: [GameEntry] = []

        for root in roots {
            guard let files = try? FileManager.default.contentsOfDirectory(
                at: root,
                includingPropertiesForKeys: [.isRegularFileKey, .fileSizeKey],
                options: [.skipsHiddenFiles]
            ) else { continue }

            for url in files {
                guard gameExtensions.contains(url.pathExtension.lowercased()) else { continue }
                guard seen.insert(url.standardizedFileURL.path.lowercased()).inserted else { continue }

                let values = try? url.resourceValues(forKeys: [.fileSizeKey])
                let size = UInt64(values?.fileSize ?? 0)
                let source = root == MacPaths.directory("iso") || root == MacPaths.dataRoot ? "Library" : root.lastPathComponent
                let fileTitle = url.deletingPathExtension().lastPathComponent
                let serial = Self.detectSerial(in: url)
                    ?? metadata.serial(forTitle: fileTitle)
                scanned.append(GameEntry(
                    id: url.standardizedFileURL.path,
                    name: url.lastPathComponent,
                    url: url,
                    size: size,
                    source: source,
                    serial: serial,
                    region: serial.flatMap { metadata.region(for: $0) },
                    coverURL: coverURL(for: url, serial: serial)
                ))
            }
        }

        games = scanned.sorted { lhs, rhs in
            lhs.displayName.localizedCaseInsensitiveCompare(rhs.displayName) == .orderedAscending
        }
        if autoDownloadCovers {
            scheduleAutoCoverDownload()
        }
    }

    func importGames(_ urls: [URL], replaceExisting: Bool) {
        let destination = MacPaths.directory("iso")
        var imported = 0
        var failed = 0

        for sourceURL in urls where gameExtensions.contains(sourceURL.pathExtension.lowercased()) {
            let destURL = destination.appendingPathComponent(sourceURL.lastPathComponent)
            do {
                if FileManager.default.fileExists(atPath: destURL.path) {
                    if replaceExisting {
                        try FileManager.default.removeItem(at: destURL)
                    } else {
                        failed += 1
                        continue
                    }
                }
                try FileManager.default.copyItem(at: sourceURL, to: destURL)
                imported += 1
            } catch {
                failed += 1
            }
        }

        lastMessage = "Imported \(imported) game(s). Failed \(failed)."
        refresh()
    }

    func addExternalFolder(_ url: URL) {
        var folders = UserDefaults.standard.stringArray(forKey: "ARMSX2MacExternalGameFolders") ?? []
        let path = url.standardizedFileURL.path
        if !folders.contains(path) {
            folders.append(path)
            UserDefaults.standard.set(folders, forKey: "ARMSX2MacExternalGameFolders")
        }
        refresh()
    }

    func removeExternalFolder(_ path: String) {
        var folders = UserDefaults.standard.stringArray(forKey: "ARMSX2MacExternalGameFolders") ?? []
        folders.removeAll { $0 == path }
        UserDefaults.standard.set(folders, forKey: "ARMSX2MacExternalGameFolders")
        refresh()
    }

    func externalGameFolders() -> [URL] {
        (UserDefaults.standard.stringArray(forKey: "ARMSX2MacExternalGameFolders") ?? [])
            .map { URL(fileURLWithPath: $0, isDirectory: true) }
    }

    func importCover(_ cover: URL, for game: GameEntry) {
        let destination = managedCoverDestination(for: game, extension: cover.pathExtension.isEmpty ? "png" : cover.pathExtension)
        do {
            if FileManager.default.fileExists(atPath: destination.path) {
                try FileManager.default.removeItem(at: destination)
            }
            try FileManager.default.copyItem(at: cover, to: destination)
            lastMessage = "Imported cover for \(game.displayName)."
        } catch {
            lastMessage = "Cover import failed: \(error.localizedDescription)"
        }
        refresh()
    }

    func downloadCover(for game: GameEntry, template: String) async {
        let result = await downloadCoverIfAvailable(for: game, template: template)
        switch result {
        case .success:
            lastMessage = "Downloaded cover for \(game.displayName)."
        case .noLookup:
            lastMessage = "No serial was found for \(game.displayName). Import a cover manually or rename the file with its PS2 serial."
        case .failed(let error):
            lastMessage = "Cover download failed for \(game.displayName): \(error)."
        }
        refresh(autoDownloadCovers: false)
    }

    func downloadMissingCovers(template: String, automatic: Bool = false) async {
        refresh(autoDownloadCovers: false)
        let targets = games.filter { $0.coverURL == nil }
        guard !targets.isEmpty else {
            if !automatic {
                lastMessage = "No missing covers found."
            }
            return
        }

        var downloaded = 0
        var noLookup = 0
        var failed = 0
        for game in targets {
            switch await downloadCoverIfAvailable(for: game, template: template) {
            case .success:
                downloaded += 1
            case .noLookup:
                noLookup += 1
            case .failed:
                failed += 1
            }
        }

        if !automatic || downloaded > 0 {
            lastMessage = "Downloaded \(downloaded) cover(s). Missing serial \(noLookup). Failed \(failed)."
        }
        refresh(autoDownloadCovers: false)
    }

    private func downloadCoverIfAvailable(for game: GameEntry, template: String) async -> CoverDownloadResult {
        let urls = coverDownloadURLs(for: game, template: template)
        guard !urls.isEmpty else {
            return .noLookup
        }

        var lastError: String?
        for url in urls {
            do {
                let (data, response) = try await URLSession.shared.data(from: url)
                if let http = response as? HTTPURLResponse, !(200...299).contains(http.statusCode) {
                    lastError = "HTTP \(http.statusCode)"
                    continue
                }
                guard Self.isValidCoverData(data) else {
                    lastError = "download was not an image"
                    continue
                }
                let ext = url.pathExtension.isEmpty ? "jpg" : url.pathExtension
                let destination = managedCoverDestination(for: game, extension: ext)
                try data.write(to: destination, options: .atomic)
                return .success
            } catch {
                lastError = error.localizedDescription
            }
        }
        return .failed(lastError ?? "no matching cover")
    }

    private enum CoverDownloadResult {
        case success
        case noLookup
        case failed(String)
    }

    private func coverURL(for gameURL: URL, serial: String?) -> URL? {
        let candidates = coverBaseCandidates(for: gameURL, serial: serial)

        let dirs = [MacPaths.directory("covers"), gameURL.deletingLastPathComponent(), MacPaths.dataRoot]
        for dir in dirs {
            guard let files = try? FileManager.default.contentsOfDirectory(at: dir, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles]) else { continue }
            for file in files where imageExtensions.contains(file.pathExtension.lowercased()) {
                let base = normalizedCoverToken(file.deletingPathExtension().lastPathComponent)
                if candidates.contains(base), Self.isValidCoverFile(file) {
                    return file
                } else if candidates.contains(base) {
                    try? FileManager.default.removeItem(at: file)
                }
            }
        }
        return nil
    }

    private func scheduleAutoCoverDownload() {
        guard !autoCoverDownloadInProgress else { return }
        let targets = games.filter { $0.coverURL == nil && !autoCoverAttemptedGameIDs.contains($0.id) }
        guard !targets.isEmpty else { return }

        targets.forEach { autoCoverAttemptedGameIDs.insert($0.id) }
        autoCoverDownloadInProgress = true
        Task { [weak self] in
            guard let self else { return }
            await self.downloadMissingCovers(template: "", automatic: true)
            self.autoCoverDownloadInProgress = false
        }
    }

    private func managedCoverDestination(for game: GameEntry, extension ext: String) -> URL {
        let base = game.serial.map(Self.normalizedSerialForCover) ?? game.displayName
        return MacPaths.directory("covers")
            .appendingPathComponent(base)
            .appendingPathExtension(ext)
    }

    private func coverDownloadURLs(for game: GameEntry, template: String) -> [URL] {
        let serials = coverSerials(for: game)
        let titles = titleVariants(for: game)
        var rawURLs: [String] = []

        let builtInTemplates = [
            "https://raw.githubusercontent.com/xlenore/ps2-covers/main/covers/default/${serial}.jpg",
            "https://raw.githubusercontent.com/xlenore/ps2-covers/main/covers/default/${serial}.png",
        ]
        for coverTemplate in builtInTemplates where !serials.isEmpty {
            for serial in serials {
                rawURLs.append(fill(coverTemplate, serial: serial, title: "", fileTitle: ""))
            }
        }

        if template.contains("${serial}") {
            for serial in serials {
                rawURLs.append(fill(template, serial: serial, title: "", fileTitle: ""))
            }
        }
        if template.contains("${title}") {
            for title in titles {
                rawURLs.append(fill(template, serial: "", title: title, fileTitle: ""))
            }
        }
        if template.contains("${filetitle}") {
            for title in titles {
                rawURLs.append(fill(template, serial: "", title: "", fileTitle: title))
            }
        }

        return uniqueStrings(rawURLs)
            .filter { !$0.contains("${") }
            .compactMap(URL.init(string:))
    }

    private func titleVariants(for game: GameEntry) -> [String] {
        uniqueStrings([
            game.displayName,
            game.name,
            game.url.deletingPathExtension().lastPathComponent,
            GameEntry.cleanedTitle(from: game.url.deletingPathExtension().lastPathComponent),
        ])
    }

    private func coverSerials(for game: GameEntry) -> [String] {
        var values: [String] = []
        if let serial = game.serial {
            values.append(contentsOf: Self.serialVariants(serial))
        }
        for title in titleVariants(for: game) {
            if let serial = metadata.serial(forTitle: title) {
                values.append(contentsOf: Self.serialVariants(serial))
            }
            for serial in metadata.serials(forTitle: title) {
                values.append(contentsOf: Self.serialVariants(serial))
            }
        }
        return uniqueStrings(values)
    }

    private func fill(_ template: String, serial: String, title: String, fileTitle: String) -> String {
        template
            .replacingOccurrences(of: "${serial}", with: urlPathEscape(serial))
            .replacingOccurrences(of: "${title}", with: urlPathEscape(title))
            .replacingOccurrences(of: "${filetitle}", with: urlPathEscape(fileTitle))
    }

    private func coverBaseCandidates(for gameURL: URL, serial: String?) -> Set<String> {
        var values = [
            gameURL.deletingPathExtension().lastPathComponent,
            gameURL.lastPathComponent,
            GameEntry.cleanedTitle(from: gameURL.deletingPathExtension().lastPathComponent),
        ]
        if let serial {
            values.append(contentsOf: Self.serialVariants(serial))
        }
        for title in uniqueStrings(values) {
            for serial in metadata.serials(forTitle: title) {
                values.append(contentsOf: Self.serialVariants(serial))
            }
        }
        return Set(values.map(normalizedCoverToken))
    }

    private func normalizedCoverToken(_ value: String) -> String {
        value
            .lowercased()
            .replacingOccurrences(of: #"[^a-z0-9]+"#, with: "", options: .regularExpression)
    }

    private func urlPathEscape(_ value: String) -> String {
        value.addingPercentEncoding(withAllowedCharacters: .urlPathAllowed) ?? value
    }

    private func uniqueStrings(_ values: [String]) -> [String] {
        var seen = Set<String>()
        return values.filter { !$0.isEmpty && seen.insert($0).inserted }
    }

    private static func isValidCoverFile(_ url: URL) -> Bool {
        guard let values = try? url.resourceValues(forKeys: [.fileSizeKey]),
              let fileSize = values.fileSize,
              fileSize > 1024,
              let data = try? Data(contentsOf: url, options: [.mappedIfSafe]) else {
            return false
        }
        return isValidCoverData(data)
    }

    private static func isValidCoverData(_ data: Data) -> Bool {
        guard data.count > 1024, let image = NSImage(data: data) else { return false }
        return image.isValid
    }

    private static func detectSerial(in url: URL) -> String? {
        let handle = try? FileHandle(forReadingFrom: url)
        defer { try? handle?.close() }
        guard let data = try? handle?.read(upToCount: 16 * 1024 * 1024), !data.isEmpty,
              let text = String(data: data, encoding: .ascii) else {
            return serialFromName(url.lastPathComponent)
        }

        if let serial = firstSerial(in: text) {
            return serial
        }
        return serialFromName(url.lastPathComponent)
    }

    private static func serialFromName(_ name: String) -> String? {
        firstSerial(in: name)
    }

    private static func firstSerial(in text: String) -> String? {
        let pattern = #"[A-Z]{4}[_-][0-9]{3}\.?[0-9]{2}"#
        guard let regex = try? NSRegularExpression(pattern: pattern),
              let match = regex.firstMatch(in: text, range: NSRange(text.startIndex..., in: text)),
              let range = Range(match.range, in: text) else {
            return nil
        }
        return String(text[range])
    }

    private static func normalizedSerialForCover(_ serial: String) -> String {
        let clean = serial.uppercased().replacingOccurrences(of: "_", with: "-")
        let parts = clean.replacingOccurrences(of: ".", with: "")
        return parts
    }

    private static func serialVariants(_ serial: String) -> [String] {
        let clean = serial.uppercased().replacingOccurrences(of: "_", with: "-")
        let noDot = clean.replacingOccurrences(of: ".", with: "")
        let underscore = clean.replacingOccurrences(of: "-", with: "_")
        return [noDot, clean, underscore, underscore.replacingOccurrences(of: ".", with: "")]
    }
}

final class GameMetadataDatabase {
    static let shared = GameMetadataDatabase()

    private var serialByTitle: [String: String] = [:]
    private var records: [TitleRecord] = []
    private var recordsByTitle: [String: [TitleRecord]] = [:]
    private var regionBySerial: [String: String] = [:]

    private struct TitleRecord {
        let title: String
        let serial: String
        let regionHint: String?
        let words: Set<String>
    }

    private init() {
        load()
    }

    func serial(forTitle title: String) -> String? {
        let titleCandidates = uniqueStrings([
            title,
            GameEntry.cleanedTitle(from: title),
            title.replacingOccurrences(of: ":", with: " - "),
            title.replacingOccurrences(of: " - ", with: ": "),
        ])

        for candidate in titleCandidates {
            let key = Self.titleKey(candidate)
            if let record = bestRecord(recordsByTitle[key], for: title) {
                return record.serial
            }
            if let serial = serialByTitle[key] {
                return serial
            }
        }

        let words = Set(Self.titleWords(title))
        guard !words.isEmpty else { return nil }

        let requestedRegion = Self.regionHint(from: title)
        var bestRecord: TitleRecord?
        var bestScore = 0
        for record in records {
            let intersection = words.intersection(record.words).count
            guard intersection > 0 else { continue }

            var score = intersection * 10
            if record.words.isSubset(of: words) || words.isSubset(of: record.words) {
                score += 4
            }
            if let requestedRegion, requestedRegion == record.regionHint {
                score += 6
            }
            if Self.titleKey(record.title).hasPrefix(Self.titleKey(GameEntry.cleanedTitle(from: title))) {
                score += 2
            }

            if score > bestScore {
                bestScore = score
                bestRecord = record
            }
        }
        return bestScore >= 20 ? bestRecord?.serial : nil
    }

    func serials(forTitle title: String) -> [String] {
        let titleCandidates = uniqueStrings([
            title,
            GameEntry.cleanedTitle(from: title),
            title.replacingOccurrences(of: ":", with: " - "),
            title.replacingOccurrences(of: " - ", with: ": "),
        ])

        var serials: [String] = []
        let requestedRegion = Self.regionHint(from: title)
        for candidate in titleCandidates {
            let key = Self.titleKey(candidate)
            if let records = recordsByTitle[key] {
                let ordered = records.sorted { lhs, rhs in
                    (lhs.regionHint == requestedRegion ? 0 : 1) < (rhs.regionHint == requestedRegion ? 0 : 1)
                }
                serials.append(contentsOf: ordered.map(\.serial))
            }
            if let serial = serialByTitle[key] {
                serials.append(serial)
            }
        }

        if serials.isEmpty {
            let words = Set(Self.titleWords(title))
            let scored = records.compactMap { record -> (score: Int, record: TitleRecord)? in
                let intersection = words.intersection(record.words).count
                guard intersection > 0 else { return nil }

                var score = intersection * 10
                if record.words.isSubset(of: words) || words.isSubset(of: record.words) {
                    score += 4
                }
                if let requestedRegion, requestedRegion == record.regionHint {
                    score += 6
                }
                if Self.titleKey(record.title).hasPrefix(Self.titleKey(GameEntry.cleanedTitle(from: title))) {
                    score += 2
                }
                return score >= 20 ? (score, record) : nil
            }
            serials.append(contentsOf: scored.sorted { $0.score > $1.score }.map { $0.record.serial })
        }

        return uniqueStrings(serials)
    }

    func region(for serial: String) -> String? {
        regionBySerial[Self.serialKey(serial)]
    }

    private func load() {
        for url in resourceCandidates(named: "RedumpDatabase", extension: "yaml") {
            parseRedump(url)
        }
        for url in resourceCandidates(named: "GameIndex", extension: "yaml") {
            parseGameIndex(url)
        }
    }

    private func parseRedump(_ url: URL) {
        guard let text = try? String(contentsOf: url, encoding: .utf8) else { return }
        var currentName: String?
        for line in text.components(separatedBy: .newlines) {
            let trimmed = line.trimmingCharacters(in: .whitespaces)
            if trimmed.hasPrefix("name: ") {
                currentName = Self.yamlValue(trimmed)
            } else if trimmed.hasPrefix("serial: "), let name = currentName {
                store(title: name, serial: Self.serialKey(Self.yamlValue(trimmed)))
                currentName = nil
            }
        }
    }

    private func parseGameIndex(_ url: URL) {
        guard let text = try? String(contentsOf: url, encoding: .utf8) else { return }
        var currentSerial: String?
        for rawLine in text.components(separatedBy: .newlines) {
            let trimmed = rawLine.trimmingCharacters(in: .whitespaces)
            if !rawLine.hasPrefix(" "), trimmed.hasSuffix(":") {
                let possibleSerial = String(trimmed.dropLast())
                currentSerial = Self.looksLikeSerial(possibleSerial) ? Self.serialKey(possibleSerial) : nil
                continue
            }

            guard let serial = currentSerial else { continue }
            if trimmed.hasPrefix("name-en:") || trimmed.hasPrefix("name:") {
                store(title: Self.yamlValue(trimmed), serial: serial)
            } else if trimmed.hasPrefix("region:") {
                regionBySerial[serial] = Self.yamlValue(trimmed)
            }
        }
    }

    private func store(title: String, serial: String) {
        let cleaned = GameEntry.cleanedTitle(from: title)
        let record = TitleRecord(
            title: title,
            serial: serial,
            regionHint: Self.regionHint(from: title),
            words: Set(Self.titleWords(cleaned))
        )
        records.append(record)
        for candidate in uniqueStrings([title, cleaned]) {
            let key = Self.titleKey(candidate)
            recordsByTitle[key, default: []].append(record)
            if serialByTitle[key] == nil {
                serialByTitle[key] = serial
            }
        }
    }

    private func bestRecord(_ records: [TitleRecord]?, for title: String) -> TitleRecord? {
        guard let records, !records.isEmpty else { return nil }
        guard let requestedRegion = Self.regionHint(from: title) else { return records.first }
        return records.first { $0.regionHint == requestedRegion } ?? records.first
    }

    private func resourceCandidates(named name: String, extension ext: String) -> [URL] {
        let fileName = "\(name).\(ext)"
        let bundle = Bundle.main.resourceURL
        let current = URL(fileURLWithPath: FileManager.default.currentDirectoryPath, isDirectory: true)
        let likelyWorktree = Bundle.main.bundleURL
            .deletingLastPathComponent()
            .deletingLastPathComponent()

        let candidates = [
            Bundle.main.url(forResource: name, withExtension: ext),
            bundle?.appendingPathComponent(fileName),
            bundle?.appendingPathComponent("resources/\(fileName)"),
            current.appendingPathComponent("bin/resources/\(fileName)"),
            likelyWorktree.appendingPathComponent("bin/resources/\(fileName)"),
        ].compactMap { $0 }

        var seen = Set<String>()
        return candidates.filter {
            FileManager.default.fileExists(atPath: $0.path) && seen.insert($0.standardizedFileURL.path).inserted
        }
    }

    private static func yamlValue(_ line: String) -> String {
        guard let colon = line.firstIndex(of: ":") else { return line }
        var value = String(line[line.index(after: colon)...]).trimmingCharacters(in: .whitespacesAndNewlines)
        if value.hasPrefix("\""), value.hasSuffix("\""), value.count >= 2 {
            value = String(value.dropFirst().dropLast())
        }
        if value.hasPrefix("'"), value.hasSuffix("'"), value.count >= 2 {
            value = String(value.dropFirst().dropLast())
        }
        return value
    }

    private static func titleKey(_ title: String) -> String {
        title
            .lowercased()
            .replacingOccurrences(of: #"[^a-z0-9]+"#, with: "", options: .regularExpression)
    }

    private static func titleWords(_ title: String) -> [String] {
        title
            .lowercased()
            .components(separatedBy: CharacterSet.alphanumerics.inverted)
            .filter { $0.count > 1 && !["the", "and", "of", "for", "with", "usa", "europe", "japan", "canada", "australia", "rev"].contains($0) }
    }

    private static func serialKey(_ serial: String) -> String {
        serial.uppercased().replacingOccurrences(of: "_", with: "-").replacingOccurrences(of: ".", with: "")
    }

    private static func looksLikeSerial(_ value: String) -> Bool {
        value.range(of: #"^[A-Z0-9]{4}-[0-9]{3}\.?[0-9]{2}$"#, options: .regularExpression) != nil
    }

    private static func regionHint(from title: String) -> String? {
        let lower = title.lowercased()
        if lower.contains("usa") || lower.contains("canada") || lower.contains("ntsc-u") {
            return "US"
        }
        if lower.contains("europe") || lower.contains("pal") || lower.contains("australia") {
            return "EU"
        }
        if lower.contains("japan") || lower.contains("ntsc-j") {
            return "JP"
        }
        if lower.contains("korea") {
            return "KR"
        }
        if lower.contains("china") {
            return "CN"
        }
        if lower.contains("asia") {
            return "ASIA"
        }
        return nil
    }

    private func uniqueStrings(_ values: [String]) -> [String] {
        var seen = Set<String>()
        return values.filter { !$0.isEmpty && seen.insert($0).inserted }
    }
}

struct BIOSRegionInfo {
    let title: String
    let code: String
    let flagAssetName: String

    static func detect(from url: URL) -> BIOSRegionInfo {
        if let romRegion = detectROMRegion(from: url) {
            return romRegion
        }

        let name = url.lastPathComponent.lowercased()
        if let fileRegion = detectFilenameRegion(from: name) {
            return fileRegion
        }

        if name.contains("japan") || name.contains("ntsc-j") || name.contains("jp") {
            return BIOSRegionInfo(title: "Japan", code: "JP", flagAssetName: "jp")
        }
        if name.contains("europe") || name.contains("pal") || name.contains("eu") {
            return BIOSRegionInfo(title: "Europe", code: "EU", flagAssetName: "eu")
        }
        if name.contains("korea") || name.contains("kr") {
            return BIOSRegionInfo(title: "Korea", code: "KR", flagAssetName: "kr")
        }
        if name.contains("china") || name.contains("cn") {
            return BIOSRegionInfo(title: "China", code: "CN", flagAssetName: "cn")
        }
        if name.contains("hong") || name.contains("hk") {
            return BIOSRegionInfo(title: "Hong Kong", code: "HK", flagAssetName: "hk")
        }
        if name.contains("australia") || name.contains("au") {
            return BIOSRegionInfo(title: "Australia", code: "AU", flagAssetName: "au")
        }
        if name.contains("usa") || name.contains("america") || name.contains("ntsc-u") || name.contains("us") {
            return BIOSRegionInfo(title: "USA", code: "US", flagAssetName: "us")
        }
        return BIOSRegionInfo(title: "Other", code: "Other", flagAssetName: "Other")
    }

    private static func detectROMRegion(from url: URL) -> BIOSRegionInfo? {
        guard let data = try? Data(contentsOf: url, options: [.mappedIfSafe]) else { return nil }
        let prefix = data.prefix(8 * 1024 * 1024)
        let text = String(decoding: prefix, as: UTF8.self)

        if let match = firstMatch(in: text, pattern: #"\b[0-9]{4}([JAEHCTXPM])[A-Z]?[0-9]{8}\b"#),
           let region = regionInfo(forROMVERCode: match) {
            return region
        }

        if let match = firstMatch(in: text, pattern: #"System ROM Version[^\r\n]*\b([JAEHCTXPM])\b"#),
           let region = regionInfo(forROMVERCode: match) {
            return region
        }

        return nil
    }

    private static func detectFilenameRegion(from name: String) -> BIOSRegionInfo? {
        if let match = firstMatch(in: name, pattern: #"(?i)(?:ps2|scph)?[-_ ]?[0-9]{4}([jaehctxpm])"#) {
            return regionInfo(forROMVERCode: match.uppercased())
        }
        return nil
    }

    private static func firstMatch(in text: String, pattern: String) -> String? {
        guard let regex = try? NSRegularExpression(pattern: pattern),
              let match = regex.firstMatch(in: text, range: NSRange(text.startIndex..., in: text)),
              match.numberOfRanges > 1,
              let range = Range(match.range(at: 1), in: text) else {
            return nil
        }
        return String(text[range])
    }

    private static func regionInfo(forROMVERCode code: String) -> BIOSRegionInfo? {
        switch code.uppercased() {
        case "J":
            return BIOSRegionInfo(title: "Japan", code: "JP", flagAssetName: "jp")
        case "A":
            return BIOSRegionInfo(title: "USA", code: "US", flagAssetName: "us")
        case "E":
            return BIOSRegionInfo(title: "Europe", code: "EU", flagAssetName: "eu")
        case "H":
            return BIOSRegionInfo(title: "Asia", code: "AS", flagAssetName: "hk")
        case "C":
            return BIOSRegionInfo(title: "China", code: "CN", flagAssetName: "cn")
        case "T":
            return BIOSRegionInfo(title: "T10K", code: "T10K", flagAssetName: "jp")
        case "X":
            return BIOSRegionInfo(title: "Test", code: "TEST", flagAssetName: "Other")
        case "P":
            return BIOSRegionInfo(title: "Free", code: "FREE", flagAssetName: "Other")
        default:
            return nil
        }
    }
}

struct ControllerPortConfig: Identifiable, Equatable {
    let id: Int
    var enabled: Bool
    var deviceIndex: Int
    var controllerType: String
    var deadzone: Double
    var axisScale: Double
    var buttonDeadzone: Double
    var largeMotorScale: Double
    var smallMotorScale: Double
    var pressureModifier: Double
    var invertLeft: Int
    var invertRight: Int

    var playerNumber: Int { id + 1 }
}

enum ControllerProfileMode: String, CaseIterable, Identifiable {
    case twoPorts
    case port1Multitap

    var id: String { rawValue }

    var title: String {
        switch self {
        case .twoPorts: return "Two Ports"
        case .port1Multitap: return "4-Player Multitap"
        }
    }
}

@MainActor
final class BIOSLibrary: ObservableObject {
    @Published var biosFiles: [URL] = []
    @Published var selectedBIOS: String
    @Published var lastMessage: String?

    private let ini: INIFile

    init(ini: INIFile) {
        self.ini = ini
        selectedBIOS = ini.string("Filenames", "BIOS", default: "")
        refresh()
    }

    func refresh() {
        let dir = MacPaths.directory("bios")
        biosFiles = (try? FileManager.default.contentsOfDirectory(at: dir, includingPropertiesForKeys: [.fileSizeKey], options: [.skipsHiddenFiles]))?
            .filter { ["bin", "rom"].contains($0.pathExtension.lowercased()) }
            .sorted { $0.lastPathComponent.localizedCaseInsensitiveCompare($1.lastPathComponent) == .orderedAscending } ?? []
    }

    func importBIOS(_ urls: [URL]) {
        var imported = 0
        let destDir = MacPaths.directory("bios")
        for url in urls where ["bin", "rom"].contains(url.pathExtension.lowercased()) {
            let dest = destDir.appendingPathComponent(url.lastPathComponent)
            do {
                if FileManager.default.fileExists(atPath: dest.path) {
                    try FileManager.default.removeItem(at: dest)
                }
                try FileManager.default.copyItem(at: url, to: dest)
                imported += 1
            } catch {}
        }
        lastMessage = "Imported \(imported) BIOS file(s)."
        refresh()
    }

    func setDefault(_ url: URL) {
        selectedBIOS = url.lastPathComponent
        ini.set("Filenames", "BIOS", selectedBIOS)
        lastMessage = "Default BIOS set to \(selectedBIOS)."
    }
}

@MainActor
final class MacSettingsStore: ObservableObject {
    @Published var ini: INIFile
    @Published var secrets: INIFile
    @Published var coverTemplate: String
    @Published var fastBoot: Bool { didSet { ini.set("GameISO", "FastBoot", fastBoot); ini.set("EmuCore", "EnableFastBoot", fastBoot) } }
    @Published var eeCoreType: Int { didSet { ini.set("EmuCore/CPU", "CoreType", eeCoreType); ini.set("EmuCore/CPU", "UseArm64Dynarec", eeCoreType == 2) } }
    @Published var iopJIT: Bool { didSet { ini.set("EmuCore/CPU/Recompiler", "EnableIOP", iopJIT) } }
    @Published var vu0JIT: Bool { didSet { ini.set("EmuCore/CPU/Recompiler", "EnableVU0", vu0JIT) } }
    @Published var vu1JIT: Bool { didSet { ini.set("EmuCore/CPU/Recompiler", "EnableVU1", vu1JIT) } }
    @Published var fastmem: Bool { didSet { ini.set("EmuCore/CPU/Recompiler", "EnableFastmem", fastmem) } }
    @Published var mtvu: Bool { didSet { ini.set("EmuCore/Speedhacks", "vuThread", mtvu) } }
    @Published var enableCheats: Bool { didSet { ini.set("EmuCore", "EnableCheats", enableCheats) } }
    @Published var enablePatches: Bool { didSet { ini.set("EmuCore", "EnablePatches", enablePatches) } }
    @Published var enableGameFixes: Bool { didSet { ini.set("EmuCore", "EnableGameFixes", enableGameFixes) } }
    @Published var enableGameDBHardwareFixes: Bool { didSet { ini.set("EmuCore/GS", "UserHacks", !enableGameDBHardwareFixes) } }
    @Published var renderer: Int { didSet { ini.set("EmuCore/GS", "Renderer", renderer) } }
    @Published var upscale: Double { didSet { ini.set("EmuCore/GS", "upscale_multiplier", upscale) } }
    @Published var textureFiltering: Int { didSet { ini.set("EmuCore/GS", "filter", textureFiltering) } }
    @Published var hardwareMipmapping: Bool { didSet { ini.set("EmuCore/GS", "hw_mipmap", hardwareMipmapping) } }
    @Published var fxaa: Bool { didSet { ini.set("EmuCore/GS", "fxaa", fxaa) } }
    @Published var casMode: Int { didSet { ini.set("EmuCore/GS", "CASMode", casMode) } }
    @Published var interlaceMode: Int { didSet { ini.set("EmuCore/GS", "deinterlace_mode", interlaceMode) } }
    @Published var aspectRatio: String { didSet { ini.set("EmuCore/GS", "AspectRatio", aspectRatio) } }
    @Published var blendingAccuracy: Int { didSet { ini.set("EmuCore/GS", "accurate_blending_unit", blendingAccuracy) } }
    @Published var trilinearFiltering: Int { didSet { ini.set("EmuCore/GS", "TriFilter", trilinearFiltering) } }
    @Published var roundSprite: Int { didSet { ini.set("EmuCore/GS", "UserHacks_round_sprite_offset", roundSprite) } }
    @Published var alignSprite: Bool { didSet { ini.set("EmuCore/GS", "UserHacks_align_sprite_X", alignSprite) } }
    @Published var mergeSprite: Bool { didSet { ini.set("EmuCore/GS", "UserHacks_merge_pp_sprite", mergeSprite) } }
    @Published var wildArmsOffset: Bool { didSet { ini.set("EmuCore/GS", "UserHacks_ForceEvenSpritePosition", wildArmsOffset) } }
    @Published var textureOffsetX: Int { didSet { ini.set("EmuCore/GS", "UserHacks_TCOffsetX", textureOffsetX) } }
    @Published var textureOffsetY: Int { didSet { ini.set("EmuCore/GS", "UserHacks_TCOffsetY", textureOffsetY) } }
    @Published var halfPixelOffset: Int { didSet { ini.set("EmuCore/GS", "UserHacks_HalfPixelOffset", halfPixelOffset) } }
    @Published var skipDrawStart: Int { didSet { ini.set("EmuCore/GS", "UserHacks_SkipDraw_Start", skipDrawStart) } }
    @Published var skipDrawEnd: Int { didSet { ini.set("EmuCore/GS", "UserHacks_SkipDraw_End", skipDrawEnd) } }
    @Published var loadTextureReplacements: Bool { didSet { ini.set("EmuCore/GS", "LoadTextureReplacements", loadTextureReplacements) } }
    @Published var asyncTextureReplacements: Bool { didSet { ini.set("EmuCore/GS", "LoadTextureReplacementsAsync", asyncTextureReplacements) } }
    @Published var precacheTextureReplacements: Bool { didSet { ini.set("EmuCore/GS", "PrecacheTextureReplacements", precacheTextureReplacements) } }
    @Published var dumpReplaceableTextures: Bool { didSet { ini.set("EmuCore/GS", "DumpReplaceableTextures", dumpReplaceableTextures) } }
    @Published var dumpReplaceableMipmaps: Bool { didSet { ini.set("EmuCore/GS", "DumpReplaceableMipmaps", dumpReplaceableMipmaps) } }
    @Published var dumpTexturesWithFMVActive: Bool { didSet { ini.set("EmuCore/GS", "DumpTexturesWithFMVActive", dumpTexturesWithFMVActive) } }
    @Published var showTextureReplacementStatus: Bool { didSet { ini.set("EmuCore/GS", "OsdShowTextureReplacements", showTextureReplacementStatus) } }
    @Published var osdPreset: Int { didSet { ini.set("ARMSX2Mac/UI", "OsdPreset", osdPreset); applyOSDPreset(osdPreset) } }
    @Published var frameLimiterEnabled: Bool { didSet { ini.set("EmuCore/Framerate", "NominalScalar", frameLimiterEnabled ? 1.0 : 0.0) } }
    @Published var fastCDVD: Bool { didSet { ini.set("EmuCore/Speedhacks", "fastCDVD", fastCDVD) } }
    @Published var eeCycleRate: Int { didSet { ini.set("EmuCore/Speedhacks", "EECycleRate", eeCycleRate) } }
    @Published var vu1Instant: Bool { didSet { ini.set("EmuCore/Speedhacks", "vu1Instant", vu1Instant) } }
    @Published var waitLoop: Bool { didSet { ini.set("EmuCore/Speedhacks", "WaitLoop", waitLoop) } }
    @Published var intcStat: Bool { didSet { ini.set("EmuCore/Speedhacks", "IntcStat", intcStat) } }
    @Published var widescreenPatches: Bool { didSet { ini.set("EmuCore", "EnableWideScreenPatches", widescreenPatches) } }
    @Published var noInterlacingPatches: Bool { didSet { ini.set("EmuCore", "EnableNoInterlacingPatches", noInterlacingPatches) } }
    @Published var hostFilesystem: Bool { didSet { ini.set("EmuCore", "HostFs", hostFilesystem) } }
    @Published var achievementsEnabled: Bool { didSet { ini.set("Achievements", "Enabled", achievementsEnabled) } }
    @Published var achievementsHardcore: Bool { didSet { ini.set("Achievements", "ChallengeMode", achievementsHardcore) } }
    @Published var achievementsNotifications: Bool { didSet { ini.set("Achievements", "Notifications", achievementsNotifications) } }
    @Published var achievementsLeaderboardNotifications: Bool { didSet { ini.set("Achievements", "LeaderboardNotifications", achievementsLeaderboardNotifications) } }
    @Published var achievementsSoundEffects: Bool { didSet { ini.set("Achievements", "SoundEffects", achievementsSoundEffects) } }
    @Published var achievementsOverlays: Bool { didSet { ini.set("Achievements", "Overlays", achievementsOverlays) } }
    @Published var achievementsLeaderboardOverlays: Bool { didSet { ini.set("Achievements", "LBOverlays", achievementsLeaderboardOverlays) } }
    @Published var achievementsEncore: Bool { didSet { ini.set("Achievements", "EncoreMode", achievementsEncore) } }
    @Published var achievementsUnofficial: Bool { didSet { ini.set("Achievements", "UnofficialTestMode", achievementsUnofficial) } }
    @Published var achievementsSpectator: Bool { didSet { ini.set("Achievements", "SpectatorMode", achievementsSpectator) } }
    @Published var achievementsNotificationsDuration: Int { didSet { ini.set("Achievements", "NotificationsDuration", achievementsNotificationsDuration) } }
    @Published var achievementsLeaderboardsDuration: Int { didSet { ini.set("Achievements", "LeaderboardsDuration", achievementsLeaderboardsDuration) } }
    @Published var retroAchievementsUsername: String
    @Published var retroAchievementsPassword = ""
    @Published var retroAchievementsToken: String
    @Published var retroAchievementsStatus: String
    @Published var retroAchievementsLoginInProgress = false
    @Published var dev9Enabled: Bool { didSet { ini.set("DEV9", "Enabled", dev9Enabled) } }
    @Published var sdlInputEnabled: Bool { didSet { ini.set("InputSources", "SDL", sdlInputEnabled) } }
    @Published var sdlEnhancedMode: Bool { didSet { ini.set("InputSources", "SDLControllerEnhancedMode", sdlEnhancedMode) } }
    @Published var sdlPlayerLED: Bool { didSet { ini.set("InputSources", "SDLPS5PlayerLED", sdlPlayerLED) } }
    @Published var controllerProfileMode: ControllerProfileMode { didSet { saveControllerPorts() } }
    @Published var controllerPorts: [ControllerPortConfig] { didSet { saveControllerPorts() } }
    @Published var detectedControllerNames: [String]
    @Published var controllerStatus: String = ""
    @Published var inGameQuickMenuHotkeyEnabled: Bool { didSet { ini.set("ARMSX2Mac/UI", "InGameQuickMenuHotkey", inGameQuickMenuHotkeyEnabled) } }
    @Published var lastMessage: String?

    init() {
        MacPaths.ensureLayout()
        let ini = INIFile(url: MacPaths.settingsFile)
        let secrets = INIFile(url: MacPaths.secretsFile)
        self.ini = ini
        self.secrets = secrets
        coverTemplate = UserDefaults.standard.string(forKey: "ARMSX2MacCoverTemplate")
            ?? "https://raw.githubusercontent.com/xlenore/ps2-covers/main/covers/default/${serial}.jpg"
        fastBoot = ini.bool("GameISO", "FastBoot", default: true)
        eeCoreType = ini.int("EmuCore/CPU", "CoreType", default: 2)
        iopJIT = ini.bool("EmuCore/CPU/Recompiler", "EnableIOP", default: true)
        vu0JIT = ini.bool("EmuCore/CPU/Recompiler", "EnableVU0", default: true)
        vu1JIT = ini.bool("EmuCore/CPU/Recompiler", "EnableVU1", default: true)
        fastmem = ini.bool("EmuCore/CPU/Recompiler", "EnableFastmem", default: true)
        mtvu = ini.bool("EmuCore/Speedhacks", "vuThread", default: false)
        enableCheats = ini.bool("EmuCore", "EnableCheats", default: false)
        enablePatches = ini.bool("EmuCore", "EnablePatches", default: true)
        enableGameFixes = ini.bool("EmuCore", "EnableGameFixes", default: true)
        enableGameDBHardwareFixes = !ini.bool("EmuCore/GS", "UserHacks", default: false)
        renderer = Self.supportedMacRenderer(ini.int("EmuCore/GS", "Renderer", default: 17))
        upscale = ini.float("EmuCore/GS", "upscale_multiplier", default: 2.0)
        textureFiltering = ini.int("EmuCore/GS", "filter", default: 1)
        hardwareMipmapping = ini.bool("EmuCore/GS", "hw_mipmap", default: true)
        fxaa = ini.bool("EmuCore/GS", "fxaa", default: false)
        casMode = ini.int("EmuCore/GS", "CASMode", default: 0)
        interlaceMode = ini.int("EmuCore/GS", "deinterlace_mode", default: 7)
        aspectRatio = ini.string("EmuCore/GS", "AspectRatio", default: "Auto 4:3/3:2")
        blendingAccuracy = ini.int("EmuCore/GS", "accurate_blending_unit", default: 1)
        trilinearFiltering = ini.int("EmuCore/GS", "TriFilter", default: 1)
        roundSprite = ini.int("EmuCore/GS", "UserHacks_round_sprite_offset", default: 0)
        alignSprite = ini.bool("EmuCore/GS", "UserHacks_align_sprite_X", default: false)
        mergeSprite = ini.bool("EmuCore/GS", "UserHacks_merge_pp_sprite", default: false)
        wildArmsOffset = ini.bool("EmuCore/GS", "UserHacks_ForceEvenSpritePosition", default: false)
        textureOffsetX = ini.int("EmuCore/GS", "UserHacks_TCOffsetX", default: 0)
        textureOffsetY = ini.int("EmuCore/GS", "UserHacks_TCOffsetY", default: 0)
        halfPixelOffset = ini.int("EmuCore/GS", "UserHacks_HalfPixelOffset", default: 0)
        skipDrawStart = ini.int("EmuCore/GS", "UserHacks_SkipDraw_Start", default: 0)
        skipDrawEnd = ini.int("EmuCore/GS", "UserHacks_SkipDraw_End", default: 0)
        loadTextureReplacements = ini.bool("EmuCore/GS", "LoadTextureReplacements", default: false)
        asyncTextureReplacements = ini.bool("EmuCore/GS", "LoadTextureReplacementsAsync", default: true)
        precacheTextureReplacements = ini.bool("EmuCore/GS", "PrecacheTextureReplacements", default: false)
        dumpReplaceableTextures = ini.bool("EmuCore/GS", "DumpReplaceableTextures", default: false)
        dumpReplaceableMipmaps = ini.bool("EmuCore/GS", "DumpReplaceableMipmaps", default: false)
        dumpTexturesWithFMVActive = ini.bool("EmuCore/GS", "DumpTexturesWithFMVActive", default: false)
        showTextureReplacementStatus = ini.bool("EmuCore/GS", "OsdShowTextureReplacements", default: false)
        osdPreset = ini.int("ARMSX2Mac/UI", "OsdPreset", default: 1)
        frameLimiterEnabled = ini.float("EmuCore/Framerate", "NominalScalar", default: 1.0) > 0.0
        fastCDVD = ini.bool("EmuCore/Speedhacks", "fastCDVD", default: false)
        eeCycleRate = ini.int("EmuCore/Speedhacks", "EECycleRate", default: 0)
        vu1Instant = ini.bool("EmuCore/Speedhacks", "vu1Instant", default: false)
        waitLoop = ini.bool("EmuCore/Speedhacks", "WaitLoop", default: true)
        intcStat = ini.bool("EmuCore/Speedhacks", "IntcStat", default: true)
        widescreenPatches = ini.bool("EmuCore", "EnableWideScreenPatches", default: false)
        noInterlacingPatches = ini.bool("EmuCore", "EnableNoInterlacingPatches", default: false)
        hostFilesystem = ini.bool("EmuCore", "HostFs", default: false)
        achievementsEnabled = ini.bool("Achievements", "Enabled", default: false)
        achievementsHardcore = ini.bool("Achievements", "ChallengeMode", default: false)
        achievementsNotifications = ini.bool("Achievements", "Notifications", default: true)
        achievementsLeaderboardNotifications = ini.bool("Achievements", "LeaderboardNotifications", default: true)
        achievementsSoundEffects = ini.bool("Achievements", "SoundEffects", default: true)
        achievementsOverlays = ini.bool("Achievements", "Overlays", default: true)
        achievementsLeaderboardOverlays = ini.bool("Achievements", "LBOverlays", default: true)
        achievementsEncore = ini.bool("Achievements", "EncoreMode", default: false)
        achievementsUnofficial = ini.bool("Achievements", "UnofficialTestMode", default: false)
        achievementsSpectator = ini.bool("Achievements", "SpectatorMode", default: false)
        achievementsNotificationsDuration = ini.int("Achievements", "NotificationsDuration", default: 5)
        achievementsLeaderboardsDuration = ini.int("Achievements", "LeaderboardsDuration", default: 5)
        let savedRAUsername = ini.string("Achievements", "Username", default: "")
        let savedRAToken = secrets.string("Achievements", "Token", default: "")
        retroAchievementsUsername = savedRAUsername
        retroAchievementsToken = savedRAToken
        retroAchievementsStatus = savedRAUsername.isEmpty || savedRAToken.isEmpty
            ? "Not logged in"
            : "Logged in as \(savedRAUsername)"
        dev9Enabled = ini.bool("DEV9", "Enabled", default: false)
        sdlInputEnabled = ini.bool("InputSources", "SDL", default: true)
        sdlEnhancedMode = ini.bool("InputSources", "SDLControllerEnhancedMode", default: true)
        sdlPlayerLED = ini.bool("InputSources", "SDLPS5PlayerLED", default: true)
        let savedControllerMode: ControllerProfileMode = ini.bool("Pad", "MultitapPort1", default: false) ? .port1Multitap : .twoPorts
        controllerProfileMode = savedControllerMode
        controllerPorts = Self.loadControllerPorts(from: ini, mode: savedControllerMode)
        detectedControllerNames = Self.currentControllerNames()
        controllerStatus = "Controllers ready."
        inGameQuickMenuHotkeyEnabled = ini.bool("ARMSX2Mac/UI", "InGameQuickMenuHotkey", default: true)
        writeBrandingDefaults()
        ini.set("EmuCore/GS", "Renderer", renderer)
        ensureHotkeyDefaults()
        applyOSDPreset(osdPreset)
        ensureControllerDefaultsIfNeeded()
    }

    func saveCoverTemplate() {
        UserDefaults.standard.set(coverTemplate, forKey: "ARMSX2MacCoverTemplate")
    }

    func openTextureFolder() {
        NSWorkspace.shared.open(MacPaths.directory("textures"))
    }

    func importTexturePacks(_ urls: [URL]) {
        guard !urls.isEmpty else { return }

        let textureRoot = MacPaths.directory("textures")
        var imported = 0
        var failed = 0

        for source in urls {
            let accessed = source.startAccessingSecurityScopedResource()
            defer {
                if accessed {
                    source.stopAccessingSecurityScopedResource()
                }
            }

            do {
                if source.pathExtension.lowercased() == "zip" {
                    let destination = textureRoot.appendingPathComponent(source.deletingPathExtension().lastPathComponent, isDirectory: true)
                    try replaceExistingItem(at: destination)
                    try FileManager.default.createDirectory(at: destination, withIntermediateDirectories: true)
                    try unzipTexturePack(source, to: destination)
                } else {
                    let isDirectory = (try? source.resourceValues(forKeys: [.isDirectoryKey]).isDirectory) ?? source.hasDirectoryPath
                    let destination = textureRoot.appendingPathComponent(source.lastPathComponent, isDirectory: isDirectory)
                    try replaceExistingItem(at: destination)
                    try FileManager.default.copyItem(at: source, to: destination)
                }
                imported += 1
            } catch {
                failed += 1
            }
        }

        if imported > 0 {
            loadTextureReplacements = true
        }

        if failed == 0 {
            lastMessage = "Imported \(imported) texture pack item\(imported == 1 ? "" : "s")."
        } else {
            lastMessage = "Imported \(imported) texture pack item\(imported == 1 ? "" : "s"); \(failed) failed."
        }
    }

    func loginRetroAchievements() async {
        let username = retroAchievementsUsername.trimmingCharacters(in: .whitespacesAndNewlines)
        let password = retroAchievementsPassword
        guard !username.isEmpty, !password.isEmpty else {
            retroAchievementsStatus = "Enter username and password."
            return
        }

        retroAchievementsLoginInProgress = true
        defer { retroAchievementsLoginInProgress = false }

        do {
            var request = URLRequest(url: URL(string: "https://retroachievements.org/dorequest.php")!)
            request.httpMethod = "POST"
            request.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
            request.httpBody = Self.formEncoded([
                "r": "login2",
                "u": username,
                "p": password,
            ])

            let (data, response) = try await URLSession.shared.data(for: request)
            if let http = response as? HTTPURLResponse, !(200...299).contains(http.statusCode) {
                retroAchievementsStatus = "Login failed: HTTP \(http.statusCode)."
                return
            }

            guard let json = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
                retroAchievementsStatus = "Login failed: invalid response."
                return
            }

            guard Self.successValue(json["Success"]) else {
                let error = (json["Error"] as? String) ?? "invalid credentials"
                retroAchievementsStatus = "Login failed: \(error)."
                return
            }

            guard let token = json["Token"] as? String, !token.isEmpty else {
                retroAchievementsStatus = "Login failed: no token returned."
                return
            }

            let displayName = (json["User"] as? String) ?? username
            retroAchievementsUsername = displayName
            retroAchievementsToken = token
            retroAchievementsPassword = ""
            achievementsEnabled = true
            ini.set("Achievements", "Enabled", true)
            ini.set("Achievements", "Username", displayName)
            ini.set("Achievements", "LoginTimestamp", Int(Date().timeIntervalSince1970))
            secrets.set("Achievements", "Token", token)
            retroAchievementsStatus = "Logged in as \(displayName)"
        } catch {
            retroAchievementsStatus = "Login failed: \(error.localizedDescription)."
        }
    }

    func saveRetroAchievementsToken() {
        let username = retroAchievementsUsername.trimmingCharacters(in: .whitespacesAndNewlines)
        let token = retroAchievementsToken.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !username.isEmpty, !token.isEmpty else {
            retroAchievementsStatus = "Enter username and token."
            return
        }

        achievementsEnabled = true
        ini.set("Achievements", "Enabled", true)
        ini.set("Achievements", "Username", username)
        ini.set("Achievements", "LoginTimestamp", Int(Date().timeIntervalSince1970))
        secrets.set("Achievements", "Token", token)
        retroAchievementsStatus = "Logged in as \(username)"
    }

    func logoutRetroAchievements() {
        retroAchievementsToken = ""
        retroAchievementsPassword = ""
        ini.delete("Achievements", "Username")
        ini.delete("Achievements", "LoginTimestamp")
        secrets.delete("Achievements", "Token")
        retroAchievementsStatus = "Not logged in"
    }

    func refreshControllers() {
        detectedControllerNames = Self.currentControllerNames()
        controllerStatus = detectedControllerNames.isEmpty
            ? "No controllers detected."
            : "\(detectedControllerNames.count) controller\(detectedControllerNames.count == 1 ? "" : "s") detected."
    }

    func applyStandardControllerMappings() {
        saveControllerPorts()
        controllerStatus = "Applied standard controller mappings."
    }

    func resetControllerDefaults() {
        sdlInputEnabled = true
        sdlEnhancedMode = true
        sdlPlayerLED = true
        controllerProfileMode = .twoPorts
        controllerPorts = [
            Self.defaultControllerPort(id: 0, enabled: true),
            Self.defaultControllerPort(id: 1, enabled: false),
            Self.defaultControllerPort(id: 2, enabled: false),
            Self.defaultControllerPort(id: 3, enabled: false),
        ]
        saveControllerPorts()
        controllerStatus = "Controller defaults restored."
    }

    func saveControllerPorts() {
        ini.set("InputSources", "Keyboard", true)
        ini.set("InputSources", "Pointer", true)
        ini.set("InputSources", "SDL", sdlInputEnabled)
        ini.set("InputSources", "SDLControllerEnhancedMode", sdlEnhancedMode)
        ini.set("InputSources", "SDLPS5PlayerLED", sdlPlayerLED)
        ini.set("Pad", "MultitapPort1", controllerProfileMode == .port1Multitap)
        ini.set("Pad", "MultitapPort2", false)
        ini.set("Pad", "PointerXScale", 8.0)
        ini.set("Pad", "PointerYScale", 8.0)

        let activeSections = activePadSections()
        for padIndex in 0..<8 {
            let section = "Pad\(padIndex + 1)"
            clearDualShockBindings(in: section)
            guard let port = activeSections[section], port.enabled else {
                ini.set(section, "Type", "None")
                continue
            }

            ini.set(section, "Type", port.controllerType)
            ini.set(section, "InvertL", port.invertLeft)
            ini.set(section, "InvertR", port.invertRight)
            ini.set(section, "Deadzone", port.deadzone)
            ini.set(section, "AxisScale", port.axisScale)
            ini.set(section, "LargeMotorScale", port.largeMotorScale)
            ini.set(section, "SmallMotorScale", port.smallMotorScale)
            ini.set(section, "ButtonDeadzone", port.buttonDeadzone)
            ini.set(section, "PressureModifier", port.pressureModifier)
            writeStandardSDLBindings(in: section, deviceIndex: port.deviceIndex)
        }
    }

    func ensureUsableOSDPreset() {
        if osdPreset == 0 {
            osdPreset = 1
        } else {
            applyOSDPreset(osdPreset)
        }
    }

    private func applyOSDPreset(_ preset: Int) {
        let clampedPreset = min(max(preset, 0), 3)
        if clampedPreset != preset {
            osdPreset = clampedPreset
            return
        }

        ini.set("EmuCore/GS", "OsdScale", 85.0)
        ini.set("EmuCore/GS", "OsdMargin", 12.0)
        ini.set("EmuCore/GS", "OsdMessagesPos", 1)
        ini.set("EmuCore/GS", "OsdPerformancePos", clampedPreset == 0 ? 0 : 3)
        ini.set("EmuCore/GS", "OsdBoldText", true)

        let showSimple = clampedPreset >= 1
        let showDetail = clampedPreset >= 2
        let showFull = clampedPreset >= 3

        ini.set("EmuCore/GS", "OsdShowFPS", showSimple)
        ini.set("EmuCore/GS", "OsdShowVPS", showSimple)
        ini.set("EmuCore/GS", "OsdShowSpeed", showSimple)
        ini.set("EmuCore/GS", "OsdShowVersion", showSimple)
        ini.set("EmuCore/GS", "OsdShowIndicators", showSimple)
        ini.set("EmuCore/GS", "OsdShowResolution", showDetail)
        ini.set("EmuCore/GS", "OsdShowGSStats", showDetail)
        ini.set("EmuCore/GS", "OsdShowCPU", showDetail)
        ini.set("EmuCore/GS", "OsdShowGPU", showDetail)
        ini.set("EmuCore/GS", "OsdShowHardwareInfo", showFull)
        ini.set("EmuCore/GS", "OsdShowFrameTimes", showFull)
        ini.set("EmuCore/GS", "OsdShowGPUDebug", false)
        ini.set("EmuCore/GS", "OsdShowSettings", showFull)
        ini.set("EmuCore/GS", "OsdshowPatches", showFull)
        ini.set("EmuCore/GS", "OsdShowInputs", false)
        ini.set("EmuCore/GS", "OsdShowVideoCapture", false)
        ini.set("EmuCore/GS", "OsdShowInputRec", false)
        ini.set("EmuCore/GS", "OsdShowTextureReplacements", showTextureReplacementStatus && clampedPreset > 0)
    }

    private func replaceExistingItem(at url: URL) throws {
        if FileManager.default.fileExists(atPath: url.path) {
            try FileManager.default.removeItem(at: url)
        }
    }

    private func unzipTexturePack(_ source: URL, to destination: URL) throws {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/usr/bin/ditto")
        process.arguments = ["-x", "-k", source.path, destination.path]
        try process.run()
        process.waitUntilExit()
        if process.terminationStatus != 0 {
            throw CocoaError(.fileWriteUnknown)
        }
    }

    private static func formEncoded(_ values: [String: String]) -> Data {
        let allowed = CharacterSet(charactersIn: "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~")
        let body = values.map { key, value in
            "\(escape(key, allowed: allowed))=\(escape(value, allowed: allowed))"
        }.joined(separator: "&")
        return Data(body.utf8)
    }

    private static func escape(_ value: String, allowed: CharacterSet) -> String {
        value.addingPercentEncoding(withAllowedCharacters: allowed) ?? value
    }

    private static func successValue(_ value: Any?) -> Bool {
        if let bool = value as? Bool {
            return bool
        }
        if let int = value as? Int {
            return int != 0
        }
        if let string = value as? String {
            return ["1", "true", "yes"].contains(string.lowercased())
        }
        return false
    }

    private func writeBrandingDefaults() {
        ini.set("ARMSX2Mac/UI", "Branding", MacPaths.appDisplayName)
        ini.set("OSD", "Branding", MacPaths.appDisplayName)
        ini.set("UI", "ConfirmShutdown", false)
    }

    private func ensureHotkeyDefaults() {
        let defaults = [
            "OpenPauseMenu": "Keyboard/Escape",
            "ToggleOSD": "Keyboard/F10",
            "TogglePause": "Keyboard/Space",
            "ToggleFrameLimit": "Keyboard/F4",
            "ToggleSoftwareRendering": "Keyboard/F9",
            "SaveStateToSlot": "Keyboard/F1",
            "LoadStateFromSlot": "Keyboard/F3",
            "NextSaveStateSlot": "Keyboard/F2",
            "PreviousSaveStateSlot": "Keyboard/Shift & Keyboard/F2",
            "Screenshot": "Keyboard/F8",
        ]

        for (key, value) in defaults where ini.string("Hotkeys", key, default: "").isEmpty {
            ini.set("Hotkeys", key, value)
        }
    }

    private func ensureControllerDefaultsIfNeeded() {
        let hasPadType = (0..<8).contains { !ini.string("Pad\($0 + 1)", "Type", default: "").isEmpty }
        if !hasPadType {
            saveControllerPorts()
        }
    }

    private func activePadSections() -> [String: ControllerPortConfig] {
        var result: [String: ControllerPortConfig] = [:]
        for port in controllerPorts {
            result[Self.padSection(forPlayer: port.id, mode: controllerProfileMode)] = port
        }
        return result
    }

    private func clearDualShockBindings(in section: String) {
        for binding in Self.dualShockBindingNames {
            ini.delete(section, binding)
        }
    }

    private func writeStandardSDLBindings(in section: String, deviceIndex: Int) {
        let prefix = "SDL-\(max(0, deviceIndex))"
        let mapping: [String: [String]] = [
            "Up": ["\(prefix)/DPadUp"],
            "Right": ["\(prefix)/DPadRight"],
            "Down": ["\(prefix)/DPadDown"],
            "Left": ["\(prefix)/DPadLeft"],
            "Triangle": ["\(prefix)/FaceNorth"],
            "Circle": ["\(prefix)/FaceEast"],
            "Cross": ["\(prefix)/FaceSouth"],
            "Square": ["\(prefix)/FaceWest"],
            "Select": ["\(prefix)/Back"],
            "Start": ["\(prefix)/Start"],
            "L1": ["\(prefix)/LeftShoulder"],
            "L2": ["\(prefix)/+LeftTrigger"],
            "R1": ["\(prefix)/RightShoulder"],
            "R2": ["\(prefix)/+RightTrigger"],
            "L3": ["\(prefix)/LeftStick"],
            "R3": ["\(prefix)/RightStick"],
            "Analog": ["\(prefix)/Guide"],
            "LUp": ["\(prefix)/-LeftY"],
            "LRight": ["\(prefix)/+LeftX"],
            "LDown": ["\(prefix)/+LeftY"],
            "LLeft": ["\(prefix)/-LeftX"],
            "RUp": ["\(prefix)/-RightY"],
            "RRight": ["\(prefix)/+RightX"],
            "RDown": ["\(prefix)/+RightY"],
            "RLeft": ["\(prefix)/-RightX"],
        ]
        for (key, values) in mapping {
            ini.setList(section, key, values)
        }
        ini.set(section, "LargeMotor", "\(prefix)/LargeMotor")
        ini.set(section, "SmallMotor", "\(prefix)/SmallMotor")
    }

    private static func loadControllerPorts(from ini: INIFile, mode: ControllerProfileMode) -> [ControllerPortConfig] {
        (0..<4).map { player in
            let section = padSection(forPlayer: player, mode: mode)
            let defaultEnabled = player == 0
            let type = ini.string(section, "Type", default: defaultEnabled ? "DualShock2" : "None")
            let deviceIndex = detectDeviceIndex(from: ini, section: section) ?? player
            return ControllerPortConfig(
                id: player,
                enabled: type != "None",
                deviceIndex: deviceIndex,
                controllerType: type == "None" ? "DualShock2" : type,
                deadzone: ini.float(section, "Deadzone", default: 0.0),
                axisScale: ini.float(section, "AxisScale", default: 1.33),
                buttonDeadzone: ini.float(section, "ButtonDeadzone", default: 0.0),
                largeMotorScale: ini.float(section, "LargeMotorScale", default: 1.0),
                smallMotorScale: ini.float(section, "SmallMotorScale", default: 1.0),
                pressureModifier: ini.float(section, "PressureModifier", default: 0.5),
                invertLeft: ini.int(section, "InvertL", default: 0),
                invertRight: ini.int(section, "InvertR", default: 0)
            )
        }
    }

    private static func defaultControllerPort(id: Int, enabled: Bool) -> ControllerPortConfig {
        ControllerPortConfig(
            id: id,
            enabled: enabled,
            deviceIndex: id,
            controllerType: "DualShock2",
            deadzone: 0.0,
            axisScale: 1.33,
            buttonDeadzone: 0.0,
            largeMotorScale: 1.0,
            smallMotorScale: 1.0,
            pressureModifier: 0.5,
            invertLeft: 0,
            invertRight: 0
        )
    }

    private static func padSection(forPlayer player: Int, mode: ControllerProfileMode) -> String {
        let padIndex: Int
        if mode == .port1Multitap {
            padIndex = [0, 2, 3, 4][min(max(player, 0), 3)]
        } else {
            padIndex = [0, 1, 2, 3][min(max(player, 0), 3)]
        }
        return "Pad\(padIndex + 1)"
    }

    private static func detectDeviceIndex(from ini: INIFile, section: String) -> Int? {
        for key in dualShockBindingNames {
            for value in ini.stringList(section, key) + [ini.string(section, key, default: "")] {
                if let index = deviceIndex(in: value) {
                    return index
                }
            }
        }
        return nil
    }

    private static func deviceIndex(in binding: String) -> Int? {
        guard let range = binding.range(of: #"SDL-(\d+)"#, options: .regularExpression) else { return nil }
        let match = String(binding[range])
        return Int(match.dropFirst(4))
    }

    private static func currentControllerNames() -> [String] {
        GCController.controllers().map { controller in
            controller.vendorName ?? controller.productCategory
        }
    }

    private static func supportedMacRenderer(_ renderer: Int) -> Int {
        switch renderer {
        case -1, 11, 13, 17:
            return renderer
        default:
            return 17
        }
    }

    private static let dualShockBindingNames = [
        "Up", "Right", "Down", "Left",
        "Triangle", "Circle", "Cross", "Square",
        "Select", "Start", "L1", "L2", "R1", "R2", "L3", "R3",
        "Analog", "Pressure",
        "LUp", "LRight", "LDown", "LLeft",
        "RUp", "RRight", "RDown", "RLeft",
        "LargeMotor", "SmallMotor",
    ]
}

enum BackendHotkey {
    case openPauseMenu
    case toggleOSD
    case togglePause
    case toggleFrameLimit
    case toggleSoftwareRenderer
    case saveState
    case loadState
    case screenshot

    var keyCode: CGKeyCode {
        switch self {
        case .openPauseMenu: return 53
        case .togglePause: return 49
        case .saveState: return 122
        case .loadState: return 99
        case .toggleFrameLimit: return 118
        case .screenshot: return 100
        case .toggleSoftwareRenderer: return 101
        case .toggleOSD: return 109
        }
    }
}

@MainActor
final class EmulatorLauncher: ObservableObject {
    @Published var executable: URL?
    @Published var isRunning = false
    @Published var lastMessage: String?

    private var process: Process?

    init() {
        executable = MacPaths.emulatorExecutable
    }

    func chooseExecutable() {
        let panel = NSOpenPanel()
        panel.title = "Choose PCSX2/ARMSX2 macOS executable"
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        panel.allowsMultipleSelection = false
        panel.allowedContentTypes = [.unixExecutable, .applicationBundle, .item]
        guard panel.runModal() == .OK, let url = panel.url else { return }

        let resolved = executableURL(from: url)
        executable = resolved
        UserDefaults.standard.set(resolved.path, forKey: "ARMSX2MacEmulatorExecutable")
    }

    func boot(_ game: GameEntry, fastBoot: Bool, fullscreen: Bool) {
        let backend = executable ?? MacPaths.emulatorExecutable
        guard let backend else {
            lastMessage = "No bundled macOS emulator backend was found in this app."
            return
        }

        let resolvedExecutable = executableURL(from: backend)
        guard FileManager.default.isExecutableFile(atPath: resolvedExecutable.path) else {
            lastMessage = "Emulator executable is not runnable: \(resolvedExecutable.path)"
            return
        }

        let process = Process()
        process.executableURL = resolvedExecutable
        var args = ["-nogui", "-datapath", MacPaths.dataRoot.path, fastBoot ? "-fastboot" : "-slowboot"]
        if fullscreen {
            args.append("-fullscreen")
        }
        args.append(game.url.path)
        process.arguments = args
        var environment = ProcessInfo.processInfo.environment
        environment["ARMSX2_MACOS_WRAPPER_MODE"] = "1"
        process.environment = environment
        process.currentDirectoryURL = resolvedExecutable.deletingLastPathComponent()
        process.terminationHandler = { [weak self] _ in
            Task { @MainActor in
                self?.isRunning = false
            }
        }

        do {
            try process.run()
            executable = resolvedExecutable
            UserDefaults.standard.set(resolvedExecutable.path, forKey: "ARMSX2MacEmulatorExecutable")
            self.process = process
            isRunning = true
            lastMessage = "Booting \(game.displayName)."
        } catch {
            lastMessage = "Boot failed: \(error.localizedDescription)"
        }
    }

    func bootBIOS(fullscreen: Bool) {
        let backend = executable ?? MacPaths.emulatorExecutable
        guard let backend else {
            lastMessage = "No bundled macOS emulator backend was found in this app."
            return
        }

        let resolvedExecutable = executableURL(from: backend)
        guard FileManager.default.isExecutableFile(atPath: resolvedExecutable.path) else {
            lastMessage = "Emulator executable is not runnable: \(resolvedExecutable.path)"
            return
        }

        let process = Process()
        process.executableURL = resolvedExecutable
        var args = ["-nogui", "-datapath", MacPaths.dataRoot.path, "-slowboot", "-bios"]
        if fullscreen {
            args.append("-fullscreen")
        }
        process.arguments = args
        var environment = ProcessInfo.processInfo.environment
        environment["ARMSX2_MACOS_WRAPPER_MODE"] = "1"
        process.environment = environment
        process.currentDirectoryURL = resolvedExecutable.deletingLastPathComponent()
        process.terminationHandler = { [weak self] _ in
            Task { @MainActor in
                self?.isRunning = false
            }
        }

        do {
            try process.run()
            executable = resolvedExecutable
            UserDefaults.standard.set(resolvedExecutable.path, forKey: "ARMSX2MacEmulatorExecutable")
            self.process = process
            isRunning = true
            lastMessage = "Booting BIOS."
        } catch {
            lastMessage = "BIOS boot failed: \(error.localizedDescription)"
        }
    }

    func stop() {
        process?.terminate()
        isRunning = false
    }

    func sendHotkey(_ hotkey: BackendHotkey) {
        guard let process, process.isRunning else {
            isRunning = false
            lastMessage = "No game is running."
            return
        }

        let pid = process.processIdentifier
        NSRunningApplication(processIdentifier: pid)?.activate(options: [.activateIgnoringOtherApps])

        let source = CGEventSource(stateID: .hidSystemState)
        let keyDown = CGEvent(keyboardEventSource: source, virtualKey: hotkey.keyCode, keyDown: true)
        let keyUp = CGEvent(keyboardEventSource: source, virtualKey: hotkey.keyCode, keyDown: false)
        keyDown?.postToPid(pid)
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.04) {
            keyUp?.postToPid(pid)
        }
    }

    private func executableURL(from url: URL) -> URL {
        if url.pathExtension == "app" {
            let appName = url.deletingPathExtension().lastPathComponent
            let commonNames = [appName, "pcsx2-qt", "PCSX2", "ARMSX2"]
            for name in commonNames {
                let candidate = url.appendingPathComponent("Contents/MacOS/\(name)")
                if FileManager.default.fileExists(atPath: candidate.path) {
                    return candidate
                }
            }
        }
        return url
    }
}

enum FilePanels {
    static func chooseGames() -> [URL] {
        openPanel(
            title: "Import PS2 games",
            allowsMultipleSelection: true,
            canChooseDirectories: false,
            contentTypes: [.item]
        )
    }

    static func chooseBIOS() -> [URL] {
        openPanel(
            title: "Import BIOS",
            allowsMultipleSelection: true,
            canChooseDirectories: false,
            contentTypes: [.item]
        )
    }

    static func chooseTexturePacks() -> [URL] {
        let panel = NSOpenPanel()
        panel.title = "Import texture packs"
        panel.allowsMultipleSelection = true
        panel.canChooseFiles = true
        panel.canChooseDirectories = true
        panel.allowedContentTypes = [.item]
        return panel.runModal() == .OK ? panel.urls : []
    }

    static func chooseFolder(title: String) -> URL? {
        openPanel(
            title: title,
            allowsMultipleSelection: false,
            canChooseDirectories: true,
            contentTypes: []
        ).first
    }

    static func chooseCover() -> URL? {
        openPanel(
            title: "Choose cover image",
            allowsMultipleSelection: false,
            canChooseDirectories: false,
            contentTypes: [.image]
        ).first
    }

    private static func openPanel(
        title: String,
        allowsMultipleSelection: Bool,
        canChooseDirectories: Bool,
        contentTypes: [UTType]
    ) -> [URL] {
        let panel = NSOpenPanel()
        panel.title = title
        panel.allowsMultipleSelection = allowsMultipleSelection
        panel.canChooseFiles = !canChooseDirectories
        panel.canChooseDirectories = canChooseDirectories
        if !contentTypes.isEmpty {
            panel.allowedContentTypes = contentTypes
        }
        return panel.runModal() == .OK ? panel.urls : []
    }
}
