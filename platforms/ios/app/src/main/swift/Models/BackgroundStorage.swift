// BackgroundStorage.swift — Library background file storage and import
// SPDX-License-Identifier: GPL-3.0+

import Foundation

enum BackgroundStorage {
    static let storageDirectoryName = "backgrounds"
    static let maxWarningBytes = 100 * 1024 * 1024

    static var storageDirectory: URL {
        let fm = FileManager.default
        let base = fm.urls(for: .applicationSupportDirectory, in: .userDomainMask).first
            ?? fm.urls(for: .documentDirectory, in: .userDomainMask).first
            ?? fm.temporaryDirectory
        let dir = base.appendingPathComponent("ARMSX2", isDirectory: true)
            .appendingPathComponent(storageDirectoryName, isDirectory: true)
        try? fm.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir
    }

    static func fileURL(for asset: BackgroundAsset) -> URL {
        storageDirectory.appendingPathComponent(asset.filename, isDirectory: false)
    }

    static func exists(_ asset: BackgroundAsset) -> Bool {
        FileManager.default.fileExists(atPath: fileURL(for: asset).path)
    }

    static func importFile(from url: URL, replacing existing: BackgroundAsset?) throws -> BackgroundAsset {
        let kind = assetKind(for: url)
        let filename = filename(for: url, replacing: existing)
        let dest = storageDirectory.appendingPathComponent(filename, isDirectory: false)
        let temp = storageDirectory.appendingPathComponent(UUID().uuidString, isDirectory: false)
            .appendingPathExtension("tmp")
        try FileManager.default.copyItem(at: url, to: temp)
        return try install(temp: temp, at: dest, kind: kind, replacing: existing != nil)
    }

    static func importData(_ data: Data, fileExtension: String, replacing existing: BackgroundAsset?) throws -> BackgroundAsset {
        let ext = fileExtension.isEmpty ? "bin" : fileExtension
        let kind = assetKind(for: ext)
        let filename = filename(for: ext, replacing: existing)
        let dest = storageDirectory.appendingPathComponent(filename, isDirectory: false)
        let temp = storageDirectory.appendingPathComponent(UUID().uuidString, isDirectory: false)
            .appendingPathExtension("tmp")
        try data.write(to: temp, options: .atomic)
        return try install(temp: temp, at: dest, kind: kind, replacing: existing != nil)
    }

    private static func filename(for url: URL, replacing existing: BackgroundAsset?) -> String {
        existing?.filename ?? "\(UUID().uuidString)_\(url.lastPathComponent.sanitizedFilename)"
    }

    private static func filename(for ext: String, replacing existing: BackgroundAsset?) -> String {
        existing?.filename ?? "\(UUID().uuidString)_imported.\(ext.sanitizedFilename)"
    }

    private static func install(temp: URL, at dest: URL, kind: BackgroundAssetKind, replacing: Bool) throws -> BackgroundAsset {
        let fm = FileManager.default
        if replacing && fm.fileExists(atPath: dest.path) {
            do {
                try fm.replaceItem(at: dest, withItemAt: temp, backupItemName: nil, options: [], resultingItemURL: nil)
            } catch {
                try? fm.removeItem(at: temp)
                throw error
            }
        } else {
            do {
                try fm.moveItem(at: temp, to: dest)
            } catch {
                try? fm.removeItem(at: temp)
                throw error
            }
        }
        return BackgroundAsset(kind: kind, filename: dest.lastPathComponent)
    }

    static func remove(_ asset: BackgroundAsset?) {
        guard let asset else { return }
        let url = fileURL(for: asset)
        if FileManager.default.fileExists(atPath: url.path) {
            try? FileManager.default.removeItem(at: url)
        }
    }

    static func assetKind(for url: URL) -> BackgroundAssetKind { assetKind(for: url.pathExtension) }

    private static func assetKind(for pathExtension: String) -> BackgroundAssetKind {
        switch pathExtension.lowercased() {
        case "gif", "apng", "webp": return .animatedImage
        case "mp4", "mov", "m4v", "avi", "mkv", "webm": return .video
        default: return .image
        }
    }

    static func isLarge(_ data: Data) -> Bool { data.count > maxWarningBytes }
    static func isLarge(_ url: URL) -> Bool {
        guard let attrs = try? FileManager.default.attributesOfItem(atPath: url.path),
              let size = attrs[.size] as? NSNumber else { return false }
        return size.int64Value > Int64(maxWarningBytes)
    }

    static func fileSize(of asset: BackgroundAsset) -> Int64? {
        let url = fileURL(for: asset)
        guard let attrs = try? FileManager.default.attributesOfItem(atPath: url.path) else { return nil }
        return (attrs[.size] as? NSNumber)?.int64Value
    }

    static func migrateLegacyBackgroundsIfNeeded() {
        let defaults = UserDefaults.standard
        guard defaults.object(forKey: "ARMSX2iOSLibraryBackgroundPath") != nil else { return }

        let primaryPath = defaults.string(forKey: "ARMSX2iOSLibraryBackgroundPath") ?? ""
        let landscapePath = defaults.string(forKey: "ARMSX2iOSLibraryLandscapeBackgroundPath") ?? ""
        let dim = defaults.object(forKey: "ARMSX2iOSLibraryBackgroundDim") as? Double ?? 0.35

        var primaryAsset: BackgroundAsset?
        var landscapeAsset: BackgroundAsset?
        var primaryMigrated = false
        var landscapeMigrated = false

        let primaryExists = !primaryPath.isEmpty && FileManager.default.fileExists(atPath: URL(fileURLWithPath: primaryPath).path)
        let landscapeExists = !landscapePath.isEmpty && FileManager.default.fileExists(atPath: URL(fileURLWithPath: landscapePath).path)

        if primaryExists {
            do {
                primaryAsset = try importFile(from: URL(fileURLWithPath: primaryPath), replacing: nil)
                try? FileManager.default.removeItem(atPath: primaryPath)
                primaryMigrated = true
            } catch {
                NSLog("[ARMSX2 Background] legacy primary migration failed: %@", error.localizedDescription)
            }
        }
        if landscapeExists {
            do {
                landscapeAsset = try importFile(from: URL(fileURLWithPath: landscapePath), replacing: nil)
                try? FileManager.default.removeItem(atPath: landscapePath)
                landscapeMigrated = true
            } catch {
                NSLog("[ARMSX2 Background] legacy landscape migration failed: %@", error.localizedDescription)
            }
        }

        if let primaryAsset { defaults.set(try? JSONEncoder().encode(primaryAsset), forKey: "ARMSX2iOSBackgroundPrimaryAsset") }
        if let landscapeAsset { defaults.set(try? JSONEncoder().encode(landscapeAsset), forKey: "ARMSX2iOSBackgroundLandscapeAsset") }
        defaults.set(dim, forKey: "ARMSX2iOSBackgroundDim")

        if primaryMigrated || !primaryExists {
            defaults.removeObject(forKey: "ARMSX2iOSLibraryBackgroundPath")
        }
        if landscapeMigrated || !landscapeExists {
            defaults.removeObject(forKey: "ARMSX2iOSLibraryLandscapeBackgroundPath")
        }
        if (primaryMigrated || !primaryExists) && (landscapeMigrated || !landscapeExists) {
            defaults.removeObject(forKey: "ARMSX2iOSLibraryBackgroundDim")
        }
    }
}

private extension String {
    var sanitizedFilename: String {
        components(separatedBy: CharacterSet(charactersIn: ":/\\?%*|\"<>")).joined(separator: "_")
    }
}
