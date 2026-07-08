// SkinManifestImporter.swift — Additive v2 manifest package detection, validation, and storage
// SPDX-License-Identifier: GPL-3.0+
//
// Pure detection and validation for advanced (manifest v2) skin packages, plus
// isolated storage helpers that write only into a caller-provided destination.
// The skin library wires this into the existing import path: packages without a
// v2 manifest keep using the legacy importer, byte-for-byte unchanged.

import Foundation

/// Additive importer for advanced (manifest v2) skin packages.
enum SkinManifestImporter {
    /// Outcome of attempting a v2 import on a candidate package.
    enum V2ImportDecision {
        /// No v2 manifest was found; the caller should use the legacy importer.
        case notV2
        /// The package validated and was stored as an advanced manifest skin.
        case imported(V2ImportOutcome)
        /// A v2 manifest was found but the package is invalid or could not be stored.
        case failed(SkinImportError)
    }

    /// Structured result for an accepted v2 skin, used by later phases and by
    /// the import alert.
    struct V2ImportOutcome {
        let descriptor: VPadSkinDescriptor
        let warnings: [String]
        let manifestName: String?
        let manifestIdentifier: String?
        let manifestVersion: String?
        let manifestAuthor: String?
        let copiedAssetCount: Int
        let repairedParentFolder: Bool

        func asImportResult() -> VPadSkinImportResult {
            VPadSkinImportResult(
                descriptor: descriptor,
                warnings: warnings,
                importedImageCount: copiedAssetCount,
                extractedFileCount: copiedAssetCount
            )
        }
    }

    /// Classification of a package after manifest detection.
    enum Detection {
        /// No manifest, or a v1/non-v2 manifest: use the legacy importer.
        case legacy
        /// A v2 manifest decoded successfully.
        case v2(manifest: SkinManifestV2, packageRoot: URL, repairedParentFolder: Bool)
        /// A v2 marker was present but the manifest could not be parsed.
        case invalidV2(message: String)
    }

    // MARK: - Detection

    /// Cheap pre-extraction check used by the archive import flow. Returns true
    /// when manifest bytes peeked from an archive carry a v2 marker, signalling
    /// that the package should be extracted generically and routed here instead
    /// of through the legacy extractor.
    static func shouldTreatAsV2(manifestData: Data?) -> Bool {
        guard let data = manifestData else { return false }
        return hasV2Marker(data)
    }

    /// Detects and resolves a v2 manifest package starting from a source URL
    /// (an extracted directory, or a single file for loose-image imports).
    static func detectPackage(sourceURL: URL) -> Detection {
        guard isDirectory(url: sourceURL) else { return .legacy }
        let (packageRoot, repairedParentFolder, repairError) = resolvePackageRoot(sourceURL)
        if let repairError { return .invalidV2(message: repairError) }
        guard let manifestURL = manifestFile(in: packageRoot),
              let data = try? Data(contentsOf: manifestURL) else {
            return .legacy
        }
        // info.json is the advanced-skin convention: if present it must be a valid
        // v2 manifest, otherwise the package is rejected clearly instead of being
        // silently ignored. A manifest.json without v2 markers is v1/legacy.
        let isInfoJSON = manifestURL.lastPathComponent.lowercased() == "info.json"
        let tolerantlyParseable = (try? JSONSerialization.jsonObject(with: data)) != nil
        if !hasV2Marker(data) {
            if isInfoJSON {
                return .invalidV2(message: tolerantlyParseable
                    ? "info.json does not declare a v2 manifest (expected formatVersion 2 or a representations map)"
                    : "info.json is not valid JSON and could not be read as a v2 manifest")
            }
            return .legacy
        }
        do {
            let manifest = try SkinManifestV2.decode(from: data)
            return .v2(manifest: manifest, packageRoot: packageRoot, repairedParentFolder: repairedParentFolder)
        } catch {
            return .invalidV2(message: "the v2 manifest could not be parsed: \(error.localizedDescription)")
        }
    }

    /// Returns the URL of the manifest file at the top level of `directory`,
    /// preferring `info.json` over `manifest.json` (case-insensitive).
    static func manifestFile(in directory: URL) -> URL? {
        let entries = (try? FileManager.default.contentsOfDirectory(
            at: directory,
            includingPropertiesForKeys: nil,
            options: [.skipsHiddenFiles]
        )) ?? []
        for entry in entries where entry.lastPathComponent.lowercased() == "info.json" { return entry }
        for entry in entries where entry.lastPathComponent.lowercased() == "manifest.json" { return entry }
        return nil
    }

    /// Resolves the real package root, repairing the common single-parent-folder
    /// packaging mistake. Returns an error string only when the structure is
    /// ambiguous (several top-level folders each containing a manifest).
    static func resolvePackageRoot(_ candidate: URL) -> (root: URL, repaired: Bool, error: String?) {
        if manifestFile(in: candidate) != nil { return (candidate, false, nil) }
        let withManifest = immediateSubdirectories(of: candidate).filter { manifestFile(in: $0) != nil }
        if withManifest.count == 1 { return (withManifest[0], true, nil) }
        if withManifest.count > 1 {
            return (candidate, false, "the package contains several top-level folders with a manifest; flatten the archive so the manifest and assets sit at the top level")
        }
        return (candidate, false, nil)
    }

    /// True when the JSON data carries a v2 marker: an explicit formatVersion of
    /// 2, a representations map, or a game-type identifier (the latter only when
    /// the file is not also an explicit v1 schemaVersion manifest).
    static func hasV2Marker(_ data: Data) -> Bool {
        guard let object = try? JSONSerialization.jsonObject(with: data),
              let dictionary = object as? [String: Any] else {
            return false
        }
        if dictionary["representations"] != nil { return true }
        if (dictionary["formatVersion"] as? Int) == 2 { return true }
        if dictionary["gameTypeIdentifier"] != nil && dictionary["schemaVersion"] == nil { return true }
        return false
    }

    // MARK: - Validation

    struct InstallValidation {
        let errors: [String]
        let warnings: [String]
        let ignoredReservedFields: [String]
    }

    /// Runs the Phase 1 validator (structure only) plus an install-readiness
    /// check for required assets. Reserved fields surface as warnings, never as
    /// errors. Missing required control art blocks install.
    static func validateForInstall(_ manifest: SkinManifestV2, packageRoot: URL) -> InstallValidation {
        let result = SkinManifestValidator.validate(manifest, packageRoot: nil)
        var errors = result.errors.map(\.description)
        let warnings = result.warnings.map(\.description)
        let ignoredReservedFields = result.warnings
            .filter { $0.message.contains("reserved and ignored") || $0.message.contains("parsed but ignored") }
            .map { $0.location }

        for path in missingRequiredAssets(manifest, packageRoot: packageRoot) {
            let message = "required asset \"\(path)\" was not found in the package"
            if !errors.contains(message) {
                errors.append(message)
            }
        }
        return InstallValidation(errors: errors, warnings: warnings, ignoredReservedFields: ignoredReservedFields)
    }

    /// Asset paths required for a control to render (its normal art, or a
    /// thumbstick knob) that are absent from the package root.
    static func missingRequiredAssets(_ manifest: SkinManifestV2, packageRoot: URL) -> [String] {
        var required: [String] = []
        guard let representations = manifest.representations else { return [] }
        for (_, byScreenClass) in representations {
            for (_, orientationSet) in byScreenClass {
                for representation in [orientationSet.portrait, orientationSet.landscape].compactMap({ $0 }) {
                    guard let controls = representation.controls else { continue }
                    for control in controls {
                        if let normal = control.asset?.normal?.trimmingCharacters(in: .whitespacesAndNewlines), !normal.isEmpty {
                            required.append(normal)
                        }
                        if let knob = control.thumbstick?.name?.trimmingCharacters(in: .whitespacesAndNewlines), !knob.isEmpty {
                            required.append(knob)
                        }
                        // Per-direction D-pad art is optional, but a path that is
                        // present must resolve in the package or the import blocks.
                        required.append(contentsOf: control.directionAssets?.referencedPaths ?? [])
                    }
                }
            }
        }

        var missing: [String] = []
        for path in required {
            guard SkinAssetPath.isSafeRelative(path) else { continue }
            let resolved = packageRoot.appendingPathComponent(path)
            if !FileManager.default.fileExists(atPath: resolved.path), !missing.contains(path) {
                missing.append(path)
            }
        }
        return missing
    }

    // MARK: - Reporting

    /// True for validator warnings that describe reserved / v2.0-ignored fields.
    static func isReservedFieldWarning(_ message: String) -> Bool {
        message.contains("reserved and ignored") || message.contains("parsed but ignored")
    }

    /// Concise, skin-maker-friendly lines for a successful v2 import.
    static func successLines(repaired: Bool, reservedWarnings: [String], author: String?, version: String?) -> [String] {
        var lines: [String] = []
        if repaired {
            lines.append("The skin was nested inside a folder; it was imported from there.")
        }
        if !reservedWarnings.isEmpty {
            lines.append("Some advanced fields are reserved for a later version and were ignored.")
        }
        let trimmedAuthor = author?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        let trimmedVersion = version?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        let parts = [trimmedAuthor.isEmpty ? nil : "by \(trimmedAuthor)", trimmedVersion.isEmpty ? nil : "v\(trimmedVersion)"].compactMap { $0 }
        if !parts.isEmpty {
            lines.append("Manifest " + parts.joined(separator: " ") + ".")
        }
        return lines
    }

    /// Truncates a list of validation errors for a readable alert, appending an
    /// "and N more" summary when there are additional problems.
    static func summarizeErrors(_ errors: [String], limit: Int = 3) -> String {
        let visible = Array(errors.prefix(limit))
        let remaining = errors.count - visible.count
        var message = visible.joined(separator: "\n")
        if remaining > 0 {
            message += "\nand \(remaining) more."
        }
        return message
    }

    // MARK: - Storage

    struct AssetInstall {
        let copiedAssetCount: Int
        let totalFiles: Int
        let skipped: [String]
    }

    static let storedManifestFileName = "manifest-v2.json"
    static let assetExtensions: Set<String> = ["png", "jpg", "jpeg", "webp", "pdf"]
    static let manifestFileNames: Set<String> = ["info.json", "manifest.json", "manifest-v2.json"]

    /// Copies the package's asset files into `destination` (preserving safe
    /// relative paths), then writes the validated manifest as
    /// `manifest-v2.json`. Must be called only after validation passes.
    static func installValidatedPackage(
        manifest: SkinManifestV2,
        manifestData: Data?,
        packageRoot: URL,
        destination: URL,
        maxFileBytes: Int64,
        maxFiles: Int
    ) throws -> AssetInstall {
        try FileManager.default.createDirectory(at: destination, withIntermediateDirectories: true)
        let manager = FileManager.default
        var copied = 0
        var total = 0
        var skipped: [String] = []

        guard let enumerator = manager.enumerator(
            at: packageRoot,
            includingPropertiesForKeys: [.isRegularFileKey],
            options: [.skipsHiddenFiles]
        ) else {
            throw NSError(domain: "SkinManifestImporter", code: 1, userInfo: [NSLocalizedDescriptionKey: "the package could not be read"])
        }

        for case let url as URL in enumerator {
            let values = try? url.resourceValues(forKeys: [.isRegularFileKey])
            guard values?.isRegularFile == true else { continue }
            total += 1

            let baseName = url.lastPathComponent.lowercased()
            if manifestFileNames.contains(baseName) { continue }
            let fileExtension = url.pathExtension.lowercased()
            guard assetExtensions.contains(fileExtension) else { continue }
            guard let relative = relativePath(of: url, from: packageRoot), SkinAssetPath.isSafeRelative(relative) else {
                skipped.append(url.lastPathComponent)
                continue
            }

            let size = ((try? url.resourceValues(forKeys: [.fileSizeKey]))?.fileSize) ?? 0
            if Int64(size) > maxFileBytes {
                skipped.append(relative)
                continue
            }
            if copied >= maxFiles {
                skipped.append(relative)
                continue
            }

            let destinationURL = relative.split(separator: "/").reduce(into: destination) { result, component in
                result.appendPathComponent(String(component))
            }
            try? manager.createDirectory(at: destinationURL.deletingLastPathComponent(), withIntermediateDirectories: true)
            if manager.fileExists(atPath: destinationURL.path) {
                try? manager.removeItem(at: destinationURL)
            }
            do {
                try manager.copyItem(at: url, to: destinationURL)
                copied += 1
            } catch {
                skipped.append(relative)
            }
        }

        let manifestDestination = destination.appendingPathComponent(storedManifestFileName)
        let payload = manifestData ?? (try? JSONEncoder().encode(manifest)) ?? Data()
        try payload.write(to: manifestDestination, options: .atomic)

        return AssetInstall(copiedAssetCount: copied, totalFiles: total, skipped: skipped)
    }

    // MARK: - File helpers

    private static func isDirectory(url: URL) -> Bool {
        ((try? url.resourceValues(forKeys: [.isDirectoryKey]))?.isDirectory) ?? false
    }

    private static func immediateSubdirectories(of directory: URL) -> [URL] {
        let entries = (try? FileManager.default.contentsOfDirectory(
            at: directory,
            includingPropertiesForKeys: [.isDirectoryKey],
            options: [.skipsHiddenFiles]
        )) ?? []
        return entries.filter { url in
            ((try? url.resourceValues(forKeys: [.isDirectoryKey]))?.isDirectory) ?? false
        }
    }

    private static func relativePath(of url: URL, from root: URL) -> String? {
        let urlPath = url.standardizedFileURL.path
        let rootPath = root.standardizedFileURL.path
        guard urlPath.hasPrefix(rootPath) else { return nil }
        var relative = String(urlPath.dropFirst(rootPath.count))
        if relative.hasPrefix("/") { relative.removeFirst() }
        return relative.isEmpty ? nil : relative
    }
}

/// Errors surfaced when a v2 package is detected but cannot be installed.
enum SkinImportError: LocalizedError {
    case invalidManifest(message: String)
    case storageFailed(message: String)

    var errorDescription: String? {
        switch self {
        case .invalidManifest(let message):
            return "The advanced skin manifest is invalid:\n" + message
        case .storageFailed(let message):
            return "The advanced skin could not be stored: " + message
        }
    }
}
