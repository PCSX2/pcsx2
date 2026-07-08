// PatchStore.swift — Cheats & Patches Manager store
// SPDX-License-Identifier: GPL-3.0+
//
// Non-destructive manager: preserves raw .pnach files, lists parsed entries, and toggles
// labeled entries through the existing PCSX2 per-game INI enable lists ([Cheats]/Enable and
// [Patches]/Enable) plus reloadPatches(). The core enable list is the source of truth for
// on/off state; the sidecar only records where an installed file came from.

import Foundation
import SwiftUI

@MainActor
@Observable
final class PatchStore: @unchecked Sendable {
    static let shared = PatchStore()

    static let cheatsSection = "Cheats"
    static let patchesSection = "Patches"
    static let enableKey = "Enable"
    nonisolated static let defaultPatchDatabaseURLTemplate = "https://raw.githubusercontent.com/PCSX2/pcsx2_patches/main/patches/${serial}_${crc}.pnach"
    nonisolated static let defaultCheatDatabaseURLTemplate = "https://raw.githubusercontent.com/xs1l3n7x/pcsx2_cheats_collection/main/cheats/${serial}_${crc}.pnach"
    nonisolated static let defaultUltraWidescreenPatchURLTemplate = "https://raw.githubusercontent.com/henyckma/ARMSX2-UltraWidescreen-NaturalVision/main/patches/${serial}_${crc}.pnach"

    // Bundled template lists. Both patch DBs install into the same serial_CRC.pnach file,
    // so PCSX2 still scans a single merged file with no core changes.
    nonisolated static let bundledPatchDatabaseTemplates = [
        defaultPatchDatabaseURLTemplate,
        defaultUltraWidescreenPatchURLTemplate,
    ]
    nonisolated static let bundledCheatDatabaseTemplates = [
        defaultCheatDatabaseURLTemplate,
    ]

    private static let patchDatabaseURLTemplateKey = "ARMSX2iOSPatchURLTemplate"
    private static let cheatDatabaseURLTemplateKey = "ARMSX2iOSCheatURLTemplate"

    enum InstallOutcome {
        case success
        case invalid(String)
        case blockedByHardcore
        case noIdentity
        case writeFailed(String)

        var isSuccess: Bool {
            if case .success = self { return true }
            return false
        }

        var message: String {
            switch self {
            case .success: return "Patch installed. Some changes apply after restarting the game."
            case .invalid(let reason): return "This is not a valid patch: \(reason)"
            case .blockedByHardcore: return "Enabling cheats and patches is blocked while RetroAchievements Hardcore Mode is enabled."
            case .noIdentity: return "ARMSX2 could not find a safe patch location for this game."
            case .writeFailed(let reason): return "Could not save the patch: \(reason)"
            }
        }
    }

    private let fileManager = FileManager.default

    // Current game (manager UI context)
    private(set) var isoName: String = ""
    private(set) var launchContext: CheatsPatchesLaunchContext = .library
    private(set) var identityState: PatchIdentityState = .libraryAwaitingFirstLaunch
    private(set) var hasGameIdentity: Bool = false
    private(set) var canManageInstalledFiles: Bool = false
    private(set) var installed: [PatchEntry] = []
    private(set) var lastMessage: String?
    private(set) var lastMessageKind: PatchFeedbackKind = .information
    private(set) var showMessage = false
    private(set) var isDownloading = false

    private var currentSerial = ""
    private var currentCRC = ""
    private var currentTitle = ""

    var patchDatabaseURLTemplates: [String] {
        get {
            let defaults = UserDefaults.standard
            if defaults.object(forKey: Self.patchDatabaseURLTemplateKey) != nil {
                // Backward compat: the legacy key may hold a single String or a new array.
                if let arr = defaults.stringArray(forKey: Self.patchDatabaseURLTemplateKey), !arr.isEmpty {
                    return arr
                }
                // Migrate a legacy single-string value into a one-element list.
                if let single = defaults.string(forKey: Self.patchDatabaseURLTemplateKey), !single.isEmpty {
                    return [single]
                }
            }
            return Self.bundledPatchDatabaseTemplates
        }
        set {
            let cleaned = newValue
                .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
                .filter { !$0.isEmpty }
            UserDefaults.standard.set(cleaned, forKey: Self.patchDatabaseURLTemplateKey)
        }
    }

    var cheatDatabaseURLTemplates: [String] {
        get {
            let defaults = UserDefaults.standard
            if defaults.object(forKey: Self.cheatDatabaseURLTemplateKey) != nil {
                if let arr = defaults.stringArray(forKey: Self.cheatDatabaseURLTemplateKey), !arr.isEmpty {
                    return arr
                }
                if let single = defaults.string(forKey: Self.cheatDatabaseURLTemplateKey), !single.isEmpty {
                    return [single]
                }
            }
            return Self.bundledCheatDatabaseTemplates
        }
        set {
            let cleaned = newValue
                .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
                .filter { !$0.isEmpty }
            UserDefaults.standard.set(cleaned, forKey: Self.cheatDatabaseURLTemplateKey)
        }
    }

    var hasConfiguredPatchDatabase: Bool {
        patchDatabaseURLTemplates.contains { Self.isConfiguredDatabaseTemplate($0) }
    }

    var hasConfiguredCheatDatabase: Bool {
        cheatDatabaseURLTemplates.contains { Self.isConfiguredDatabaseTemplate($0) }
    }

    private init() {}

    // MARK: - Identity

    static func gameIdentityAvailable(forISO iso: String) -> Bool {
        let info = ARMSX2Bridge.gameSettings(forISO: iso)
        let crc = (info["crc"] as? String) ?? ""
        return !PadLayoutGameIdentity.normalizedCRC(crc).isEmpty
    }

    /// True when RetroAchievements Hardcore Mode is enabled or active. While true, no `.pnach`
    /// content — cheats *or* patches — may be enabled: the core also refuses to apply them, so
    /// already-enabled entries take no effect until Hardcore is off.
    static func hardcoreBlocksPnachContent() -> Bool {
        let state = ARMSX2Bridge.retroAchievementsState()
        let active = (state["hardcoreActive"] as? NSNumber)?.boolValue ?? false
        let preference = (state["hardcorePreference"] as? NSNumber)?.boolValue ?? false
        return active || preference || ARMSX2Bridge.isRetroAchievementsHardcoreActive()
    }

    // MARK: - Load installed

    func loadInstalled(forISO iso: String, launchContext: CheatsPatchesLaunchContext? = nil) {
        isoName = iso
        if let launchContext { self.launchContext = launchContext }

        let info: [String: Any]
        switch self.launchContext {
        case .library:
            info = ARMSX2Bridge.gameSettings(forISO: iso)
        case .inGame:
            guard let runtimeInfo = ARMSX2Bridge.gameSettingsForCurrentGame() else {
                currentSerial = ""
                currentCRC = ""
                currentTitle = iso
                hasGameIdentity = false
                canManageInstalledFiles = hasManagedPath(forISO: iso)
                identityState = .inGameLoading
                installed = canManageInstalledFiles ? readInstalledEntries(forISO: iso) : []
                return
            }
            info = runtimeInfo
        }

        let crcString = (info["crc"] as? String) ?? ""
        currentCRC = Self.formattedCRC(crcString)
        currentSerial = PadLayoutGameIdentity.normalizedSerial(info["serial"] as? String)
        let metadataTitle = self.launchContext == .library
            ? ARMSX2Bridge.gameMetadata(forISO: iso)["title"]
            : nil
        currentTitle = ((info["title"] as? String) ?? metadataTitle ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        if currentTitle.isEmpty { currentTitle = iso }
        hasGameIdentity = !currentCRC.isEmpty
        identityState = hasGameIdentity
            ? .known(crc: currentCRC)
            : (self.launchContext == .library ? .libraryAwaitingFirstLaunch : .inGameUnavailable)
        canManageInstalledFiles = hasManagedPath(forISO: iso)
        installed = canManageInstalledFiles ? readInstalledEntries(forISO: iso) : []
    }

    private func readInstalledEntries(forISO iso: String) -> [PatchEntry] {
        let enabledCheats = enableList(forISO: iso, isCheat: true)
        let enabledPatches = enableList(forISO: iso, isCheat: false)

        var entries: [PatchEntry] = []
        for isCheat in [true, false] {
            guard let path = managedPath(forISO: iso, asCheat: isCheat),
                  !path.isEmpty,
                  fileManager.fileExists(atPath: path) else { continue }
            let fileName = (path as NSString).lastPathComponent
            let originPair = sidecarSourceRaw(forISO: iso, fileName: fileName) ?? (source: .installed, detail: nil)
            let origin = originPair.source
            let originDetail = originPair.detail

            guard let text = try? String(contentsOfFile: path, encoding: .utf8) else {
                entries.append(unparseableEntry(path: path, fileName: fileName, isCheat: isCheat, origin: origin, detail: originDetail))
                continue
            }

            switch PnachParser.parse(text) {
            case .invalid:
                entries.append(unparseableEntry(path: path, fileName: fileName, isCheat: isCheat, origin: origin, detail: originDetail))
            case .valid(let parsed):
                let enabledNames = isCheat ? enabledCheats : enabledPatches
                for parsedEntry in parsed {
                    let enabled = parsedEntry.isLegacy
                        ? true
                        : enabledNames.contains(where: { $0.caseInsensitiveCompare(parsedEntry.name) == .orderedSame })
                    entries.append(PatchEntry(
                        id: "\(path)|\(parsedEntry.id)",
                        name: parsedEntry.name,
                        summary: parsedEntry.summary,
                        category: parsedEntry.category,
                        source: origin,
                        isCheat: isCheat,
                        fileName: fileName,
                        isLegacy: parsedEntry.isLegacy,
                        enabled: enabled,
                        sourceDetail: originDetail
                    ))
                }
            }
        }

        return entries.sorted(by: PatchEntry.displayOrdered)
    }

    private func managedPath(forISO iso: String, asCheat: Bool) -> String? {
        if launchContext == .inGame && iso == isoName {
            return ARMSX2Bridge.pnachPathForCurrentGame(asCheat: asCheat)
        }
        return ARMSX2Bridge.pnachPath(forISO: iso, asCheat: asCheat)
    }

    private func hasManagedPath(forISO iso: String) -> Bool {
        guard !iso.isEmpty || launchContext == .inGame else { return false }
        return [true, false].contains { isCheat in
            guard let path = managedPath(forISO: iso, asCheat: isCheat) else { return false }
            return !path.isEmpty
        }
    }

    private func hasUsableManagedPath(forISO iso: String) -> Bool {
        iso == isoName ? canManageInstalledFiles : hasManagedPath(forISO: iso)
    }

    private func unparseableEntry(path: String, fileName: String, isCheat: Bool, origin: PatchSource, detail: String? = nil) -> PatchEntry {
        PatchEntry(
            id: "\(path)|legacy",
            name: "",
            summary: "Installed patch file",
            category: isCheat ? .cheats : .other,
            source: origin,
            isCheat: isCheat,
            fileName: fileName,
            isLegacy: true,
            enabled: true,
            sourceDetail: detail
        )
    }

    // MARK: - Toggle (non-destructive: writes the per-game enable list only)

    func toggle(_ entry: PatchEntry) {
        guard canManageInstalledFiles, !entry.isLegacy, !entry.name.isEmpty else {
            if entry.isLegacy {
                applyFeedback("Legacy patches are managed by removing or reinstalling the file.")
            }
            return
        }

        var names = enableList(forISO: isoName, isCheat: entry.isCheat)
        if let index = names.firstIndex(where: { $0.caseInsensitiveCompare(entry.name) == .orderedSame }) {
            names.remove(at: index)
        } else {
            if Self.hardcoreBlocksPnachContent() {
                applyFeedback(InstallOutcome.blockedByHardcore.message, kind: .error)
                return
            }
            names.append(entry.name)
        }
        setEnableList(names, forISO: isoName, isCheat: entry.isCheat)

        if entry.isCheat {
            ARMSX2Bridge.setINIBool("EmuCore", key: "EnableCheats", value: true)
        }
        ARMSX2Bridge.reloadPatches()
        loadInstalled(forISO: isoName, launchContext: launchContext)
    }

    var canEnableAll: Bool {
        installed.contains { entry in
            !entry.isLegacy && !entry.name.isEmpty && !entry.enabled && !Self.hardcoreBlocksPnachContent()
        }
    }

    var canDisableAll: Bool {
        installed.contains { !$0.isLegacy && !$0.name.isEmpty && $0.enabled }
    }

    func setAllNamedEntries(enabled: Bool) {
        guard canManageInstalledFiles else { return }

        let patchNames = enabled && !Self.hardcoreBlocksPnachContent()
            ? installed.filter { !$0.isCheat && !$0.isLegacy && !$0.name.isEmpty }.map(\.name)
            : []
        let cheatNames = enabled && !Self.hardcoreBlocksPnachContent()
            ? installed.filter { $0.isCheat && !$0.isLegacy && !$0.name.isEmpty }.map(\.name)
            : []
        setEnableList(patchNames, forISO: isoName, isCheat: false)
        setEnableList(cheatNames, forISO: isoName, isCheat: true)
        if !cheatNames.isEmpty {
            ARMSX2Bridge.setINIBool("EmuCore", key: "EnableCheats", value: true)
        }
        ARMSX2Bridge.reloadPatches()
        loadInstalled(forISO: isoName, launchContext: launchContext)
    }

    // MARK: - Remove

    func removeInstalledFile(asCheat: Bool) {
        guard canManageInstalledFiles else { return }
        guard let path = managedPath(forISO: isoName, asCheat: asCheat), !path.isEmpty else { return }
        let fileName = (path as NSString).lastPathComponent
        do {
            try fileManager.removeItem(atPath: path)
        } catch {
            applyFeedback("Could not remove the patch file: \(error.localizedDescription)", kind: .error)
            return
        }
        setEnableList([], forISO: isoName, isCheat: asCheat)
        clearSidecar(forISO: isoName, fileName: fileName)
        ARMSX2Bridge.reloadPatches()
        loadInstalled(forISO: isoName, launchContext: launchContext)
        applyFeedback("Installed file removed.", kind: .success)
    }

    func removeAllInstalled() {
        guard canManageInstalledFiles else { return }
        for isCheat in [true, false] {
            if let path = managedPath(forISO: isoName, asCheat: isCheat), !path.isEmpty {
                let fileName = (path as NSString).lastPathComponent
                try? fileManager.removeItem(atPath: path)
                clearSidecar(forISO: isoName, fileName: fileName)
            }
        }
        setEnableList([], forISO: isoName, isCheat: true)
        setEnableList([], forISO: isoName, isCheat: false)
        ARMSX2Bridge.reloadPatches()
        loadInstalled(forISO: isoName, launchContext: launchContext)
        applyFeedback("All installed patches removed.", kind: .success)
    }

    /// Removes a single named entry's lines from its .pnach file, preserving every other entry.
    /// Legacy (unnamed) entries cannot be targeted individually and stay on the file-level path.
    func removeEntry(_ entry: PatchEntry) {
        guard canManageInstalledFiles, !entry.isLegacy, !entry.name.isEmpty else {
            if entry.isLegacy {
                applyFeedback("Legacy patches are managed by removing or reinstalling the file.")
            }
            return
        }
        guard let path = managedPath(forISO: isoName, asCheat: entry.isCheat),
              !path.isEmpty,
              fileManager.fileExists(atPath: path),
              let text = try? String(contentsOfFile: path, encoding: .utf8) else {
            applyFeedback("Could not read the patch file to remove this entry.", kind: .error)
            return
        }
        guard let draftIndex = Self.draftIndex(forEntryID: entry.id) else {
            applyFeedback("This entry could not be located for removal.", kind: .error)
            return
        }
        guard let rewritten = PnachParser.removingDraftBlock(text, draftIndex: draftIndex) else {
            applyFeedback("This entry could not be found in the file.", kind: .error)
            return
        }
        do {
            try writeManaged(text: rewritten, to: path)
        } catch {
            applyFeedback("Could not save the updated patch file: \(error.localizedDescription)", kind: .error)
            return
        }

        var names = enableList(forISO: isoName, isCheat: entry.isCheat)
        names.removeAll { $0.caseInsensitiveCompare(entry.name) == .orderedSame }
        setEnableList(names, forISO: isoName, isCheat: entry.isCheat)

        ARMSX2Bridge.reloadPatches()
        loadInstalled(forISO: isoName, launchContext: launchContext)
        applyFeedback("Entry removed from the \(entry.isCheat ? "cheat" : "patch") file.", kind: .success)
    }

    /// Extracts the draft index from a `PatchEntry.id` of the form "<path>|entry-N" or
    /// "<path>|legacy-N", matching the suffix `PnachParser` assigns.
    private static func draftIndex(forEntryID id: String) -> Int? {
        let token = id.components(separatedBy: "|").last ?? id
        guard let number = token.split(separator: "-").last.flatMap({ Int($0) }) else { return nil }
        return number
    }

    // MARK: - Install (shared by import and download; pure, takes an explicit ISO)

    func writePatch(text: String, forISO iso: String, asCheat: Bool, autoEnable: Bool, source: PatchSource, sourceDetail: String? = nil) -> InstallOutcome {
        guard hasUsableManagedPath(forISO: iso) else { return .noIdentity }

        let parsed: [ParsedPatchEntry]
        switch PnachParser.parse(text) {
        case .invalid(let reason): return .invalid(reason)
        case .valid(let entries): parsed = entries
        }

        guard let path = managedPath(forISO: iso, asCheat: asCheat), !path.isEmpty else {
            return .noIdentity
        }

        // Merge: when a valid file already exists, combine the new text with it so custom and
        // database entries coexist. Named blocks shared with the existing file replace the old
        // ones (update) instead of duplicating; new names are appended. A blank or unparseable
        // existing file falls back to a plain write (writeManaged still backs the old one up).
        var combinedText = text
        var existingEnabled: [String] = []
        var didMerge = false
        if fileManager.fileExists(atPath: path),
           let existing = try? String(contentsOfFile: path, encoding: .utf8),
           !existing.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty,
           case .valid = PnachParser.parse(existing) {
            combinedText = PnachParser.merging(existing: existing, new: text)
            existingEnabled = enableList(forISO: iso, isCheat: asCheat)
            didMerge = true
        }

        do {
            try writeManaged(text: combinedText, to: path)
        } catch {
            return .writeFailed(error.localizedDescription)
        }

        let fileName = (path as NSString).lastPathComponent
        if didMerge {
            // Preserve the file's original origin; only record a source if none exists yet, so
            // appending to a known file does not silently relabel where it came from.
            if sidecarSource(forISO: iso, fileName: fileName) == nil {
                recordSidecar(forISO: iso, fileName: fileName, source: source, detail: sourceDetail)
            }
        } else {
            recordSidecar(forISO: iso, fileName: fileName, source: source, detail: sourceDetail)
        }

        let newNames = parsed.filter { !$0.name.isEmpty }.map(\.name)
        // Under RetroAchievements Hardcore Mode nothing is auto-enabled (cheats or patches) —
        // the file is still written so entries can be enabled later, and the core refuses to
        // apply any .pnach content while Hardcore is active. Enabling is gated in `toggle`.
        let effectiveAutoEnable = autoEnable && !Self.hardcoreBlocksPnachContent()
        let merged: [String]
        if effectiveAutoEnable {
            // Union: keep previously enabled entries and add the newly imported ones.
            var seen = Set<String>()
            merged = (existingEnabled + newNames).filter { seen.insert($0.lowercased()).inserted }
        } else {
            // Download, or cheat import under Hardcore: preserve existing enables rather than
            // clearing them or silently enabling a cheat.
            merged = existingEnabled
        }
        setEnableList(merged, forISO: iso, isCheat: asCheat)
        if asCheat {
            ARMSX2Bridge.setINIBool("EmuCore", key: "EnableCheats", value: true)
        }
        ARMSX2Bridge.reloadPatches()
        return .success
    }

    @discardableResult
    func importURLs(_ urls: [URL], forISO iso: String, asCheat: Bool) -> String {
        guard hasUsableManagedPath(forISO: iso) else {
            applyFeedback(InstallOutcome.noIdentity.message, kind: .error)
            return InstallOutcome.noIdentity.message
        }

        var messages: [String] = []
        var anySuccess = false
        for url in urls {
            let accessing = url.startAccessingSecurityScopedResource()
            let patchText = readText(from: url)
            if accessing { url.stopAccessingSecurityScopedResource() }
            guard let patchText else {
                messages.append("\(url.lastPathComponent) could not be read as text.")
                continue
            }
            // Wrap headerless (legacy) patches in a named section so the import stays a
            // distinct, manageable entry instead of being absorbed into an existing section.
            let installText = PnachParser.wrappingLegacyPatches(patchText, header: Self.importLabel(for: url))
            let outcome = writePatch(text: installText, forISO: iso, asCheat: asCheat, autoEnable: true, source: .local)
            if outcome.isSuccess {
                messages.append("Imported \(url.lastPathComponent).")
                anySuccess = true
            } else {
                messages.append("\(url.lastPathComponent): \(outcome.message)")
            }
        }

        if anySuccess {
            // Refresh the installed list so imported entries appear immediately.
            loadInstalled(forISO: iso, launchContext: launchContext)
        }
        let result = messages.isEmpty ? "No patches imported." : messages.joined(separator: "\n")
        applyFeedback(
            anySuccess ? "Import complete.\n\(result)\nSome changes apply after restarting the game." : result,
            kind: anySuccess ? .success : .error
        )
        return result
    }

    private func readText(from url: URL) -> String? {
        guard let data = try? Data(contentsOf: url) else { return nil }
        return String(data: data, encoding: .utf8) ?? String(data: data, encoding: .ascii)
    }

    static func importLabel(for url: URL) -> String {
        let raw = (url.lastPathComponent as NSString).deletingPathExtension
            .replacingOccurrences(of: "[", with: "(")
            .replacingOccurrences(of: "]", with: ")")
            .replacingOccurrences(of: "\n", with: " ")
            .replacingOccurrences(of: "\r", with: " ")
            .trimmingCharacters(in: .whitespacesAndNewlines)
        return raw.isEmpty ? "Imported" : String(raw.prefix(48))
    }

    // MARK: - Database download

    func downloadFromDatabase(forISO iso: String, asCheat: Bool) async {
        let crc = iso == isoName ? currentCRC : Self.formattedCRC(ARMSX2Bridge.gameSettings(forISO: iso)["crc"] as? String)
        guard !crc.isEmpty else {
            applyFeedback(identityState.guidance ?? "Database matching is unavailable for this game.", kind: .information)
            return
        }

        let templates = (asCheat ? cheatDatabaseURLTemplates : patchDatabaseURLTemplates)
            .filter { Self.isConfiguredDatabaseTemplate($0) }
        guard !templates.isEmpty else {
            let kind = asCheat ? "cheat" : "patch"
            applyFeedback("No \(kind) database source is configured. Add a trusted URL in Advanced.", kind: .information)
            return
        }

        let serial = iso == isoName ? currentSerial : PadLayoutGameIdentity.normalizedSerial(ARMSX2Bridge.gameSettings(forISO: iso)["serial"] as? String)
        let title = iso == isoName ? currentTitle : (ARMSX2Bridge.gameMetadata(forISO: iso)["title"] ?? iso)

        isDownloading = true
        defer { isDownloading = false }

        var succeededSources: [String] = []
        var notFoundCount = 0
        var firstError: String?

        for template in templates {
            guard let url = resolvedDatabaseURL(template: template, serial: serial, crc: crc, title: title) else {
                continue
            }
            let sourceName = PatchSource.databaseName(forTemplate: template)

            do {
                var request = URLRequest(url: url)
                request.timeoutInterval = 10
                request.cachePolicy = .reloadIgnoringLocalCacheData
                let (data, response) = try await URLSession.shared.data(for: request)
                guard let http = response as? HTTPURLResponse else {
                    if firstError == nil { firstError = "Could not download from \(sourceName)." }
                    continue
                }
                if http.statusCode == 404 {
                    notFoundCount += 1
                    continue
                }
                guard http.statusCode == 200 else {
                    if firstError == nil { firstError = "\(sourceName) returned status \(http.statusCode)." }
                    continue
                }
                guard data.count <= Self.maxPatchDownloadBytes else {
                    if firstError == nil { firstError = "The file from \(sourceName) is too large to be a valid patch." }
                    continue
                }
                guard let text = String(data: data, encoding: .utf8) ?? String(data: data, encoding: .ascii) else {
                    if firstError == nil { firstError = "The file from \(sourceName) was not a valid patch." }
                    continue
                }
                guard case .valid = PnachParser.parse(text) else {
                    if firstError == nil { firstError = "The file from \(sourceName) was not a valid patch." }
                    continue
                }
                let outcome = writePatch(
                    text: text,
                    forISO: iso,
                    asCheat: asCheat,
                    autoEnable: false,
                    source: .database,
                    sourceDetail: sourceName
                )
                if outcome.isSuccess {
                    succeededSources.append(sourceName)
                } else {
                    if firstError == nil { firstError = "\(sourceName): \(outcome.message)" }
                }
            } catch {
                if firstError == nil { firstError = "Could not reach \(sourceName). Check your connection or URL." }
            }
        }

        let kindWord = asCheat ? "Cheat" : "Patch"
        if !succeededSources.isEmpty {
            // Refresh so newly installed entries show up without reopening the manager.
            loadInstalled(forISO: iso, launchContext: launchContext)
            let list = succeededSources.joined(separator: ", ")
            applyFeedback("\(kindWord) downloaded from \(list). Enable the entries you want.", kind: .success)
        } else if notFoundCount == templates.count {
            applyFeedback(
                asCheat
                    ? "No cheat found in any configured database."
                    : "No patch found in any configured database.",
                kind: .information
            )
        } else if let firstError {
            applyFeedback(firstError, kind: .error)
        } else {
            applyFeedback("Could not download from any configured database.", kind: .error)
        }
    }

    private func resolvedDatabaseURL(template: String, serial: String, crc: String, title: String) -> URL? {
        guard Self.isConfiguredDatabaseTemplate(template) else { return nil }
        if template.contains("${serial}") && serial.isEmpty { return nil }
        if template.contains("${crc}") && crc.isEmpty { return nil }
        if template.contains("${title}") && title.isEmpty { return nil }

        let filled = template
            .replacingOccurrences(of: "${serial}", with: urlComponent(serial))
            .replacingOccurrences(of: "${crc}", with: urlComponent(crc))
            .replacingOccurrences(of: "${title}", with: urlComponent(title))
        guard !filled.contains("${") else { return nil }
        guard let url = URL(string: filled) else { return nil }
        // Defense-in-depth: even with percent-encoded substitutions, only allow a real
        // http(s) URL whose host is not a loopback/link-local target.
        return Self.isSafeDownloadURL(url) ? url : nil
    }

    private func urlComponent(_ value: String) -> String {
        var allowed = CharacterSet.urlPathAllowed
        allowed.remove(charactersIn: "/")
        return value.addingPercentEncoding(withAllowedCharacters: allowed) ?? value
    }

    // http is permitted alongside https so existing users who configured a plain-http
    // source keep working; https remains the default and the recommendation.
    private static func isConfiguredDatabaseTemplate(_ template: String) -> Bool {
        template.hasPrefix("http://") || template.hasPrefix("https://")
    }

    /// Caps downloaded patch files so a misconfigured or hostile source cannot push an
    /// arbitrarily large payload through the single-template download path.
    private static let maxPatchDownloadBytes = 2_000_000

    /// Validates a resolved download URL for the current single-template surface: only
    /// http/https with a real, non-loopback host. Broader private-range blocking and
    /// redirect policing belong with the multi-source repository work.
    private static func isSafeDownloadURL(_ url: URL) -> Bool {
        guard let scheme = url.scheme?.lowercased(),
              scheme == "http" || scheme == "https" else { return false }
        guard let host = url.host, !host.isEmpty else { return false }
        return !isDisallowedHost(host)
    }

    private static func isDisallowedHost(_ host: String) -> Bool {
        let lower = host.lowercased()
        let candidate = lower.hasPrefix("[") && lower.hasSuffix("]")
            ? String(lower.dropFirst().dropLast())
            : lower
        if candidate == "localhost" { return true }
        // IPv4 loopback (127.0.0.0/8) and link-local (169.254.0.0/16).
        if candidate.hasPrefix("127.") { return true }
        if candidate.hasPrefix("169.254.") { return true }
        // IPv6 loopback.
        if candidate == "::1" { return true }
        return false
    }

    private static func formattedCRC(_ value: String?) -> String {
        let normalized = PadLayoutGameIdentity.normalizedCRC(value)
        guard let crc = UInt32(normalized, radix: 16), crc != 0 else { return "" }
        return String(format: "%08X", crc)
    }

    // MARK: - Managed write (backup + atomic)

    private func writeManaged(text: String, to path: String) throws {
        let url = URL(fileURLWithPath: path)
        let directory = url.deletingLastPathComponent()
        try fileManager.createDirectory(at: directory, withIntermediateDirectories: true)

        var normalized = text
            .replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")
        if !normalized.hasSuffix("\n") { normalized.append("\n") }

        // Stage to a temp file so the original is touched only on success.
        let temp = directory.appendingPathComponent("\(url.lastPathComponent).download")
        if fileManager.fileExists(atPath: temp.path) { try fileManager.removeItem(at: temp) }
        try normalized.write(to: temp, atomically: true, encoding: .utf8)

        // Back up the existing file with a fresh copy each write, before replacing it. A
        // failed backup aborts here so the original is never destroyed without a recovery copy.
        if fileManager.fileExists(atPath: path) {
            let backup = backupURL(for: url)
            if fileManager.fileExists(atPath: backup.path) { try fileManager.removeItem(at: backup) }
            try fileManager.copyItem(at: url, to: backup)
            try fileManager.removeItem(at: url)
        }

        try fileManager.moveItem(at: temp, to: url)
    }

    private func backupURL(for url: URL) -> URL {
        let directory = url.deletingLastPathComponent().appendingPathComponent(".backup", isDirectory: true)
        try? fileManager.createDirectory(at: directory, withIntermediateDirectories: true)
        return directory.appendingPathComponent("\(url.lastPathComponent).bak")
    }

    // MARK: - Enable list helpers

    private func enableList(forISO iso: String, isCheat: Bool) -> [String] {
        let section = isCheat ? Self.cheatsSection : Self.patchesSection
        if launchContext == .inGame && iso == isoName {
            return ARMSX2Bridge.patchEnableListForCurrentGame(section: section, key: Self.enableKey)
        }
        return ARMSX2Bridge.patchEnableList(forISO: iso, section: section, key: Self.enableKey)
    }

    private func setEnableList(_ names: [String], forISO iso: String, isCheat: Bool) {
        let section = isCheat ? Self.cheatsSection : Self.patchesSection
        if launchContext == .inGame && iso == isoName {
            ARMSX2Bridge.setPatchEnableListForCurrentGame(names, section: section, key: Self.enableKey)
        } else {
            ARMSX2Bridge.setPatchEnableList(names, forISO: iso, section: section, key: Self.enableKey)
        }
    }

    // MARK: - Sidecar (records only the origin of an installed file)

    private static let sidecarFilename = "CheatsMetadata.json"

    private var sidecarURL: URL {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first
            ?? FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first
            ?? FileManager.default.temporaryDirectory
        let directory = base.appendingPathComponent("ARMSX2", isDirectory: true)
        try? fileManager.createDirectory(at: directory, withIntermediateDirectories: true)
        return directory.appendingPathComponent(Self.sidecarFilename)
    }

    private struct PatchSidecar: Codable {
        var schemaVersion: Int = 1
        var games: [String: [String: String]] = [:]
    }

    private func loadSidecar() -> PatchSidecar {
        guard let data = try? Data(contentsOf: sidecarURL) else { return PatchSidecar() }
        return (try? JSONDecoder().decode(PatchSidecar.self, from: data)) ?? PatchSidecar()
    }

    private func saveSidecar(_ sidecar: PatchSidecar) {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        do {
            try encoder.encode(sidecar).write(to: sidecarURL, options: .atomic)
        } catch {
            NSLog("[ARMSX2 iOS Patches] sidecar save failed: %@", error.localizedDescription)
        }
    }

    private func gameID(forISO iso: String) -> String {
        if launchContext == .inGame && iso == isoName && !currentCRC.isEmpty {
            return "\(currentSerial.uppercased())|\(currentCRC)"
        }
        let info = ARMSX2Bridge.gameSettings(forISO: iso)
        let serial = ((info["serial"] as? String) ?? "").uppercased()
        let crc = PadLayoutGameIdentity.normalizedCRC((info["crc"] as? String))
        return "\(serial)|\(crc)"
    }

    // Reads the sidecar raw value and returns both the resolved source case and any DB
    // name suffix ("database:PCSX2 database" -> (.database, "PCSX2 database")). Older
    // sidecars stored a bare raw value, which still parses.
    private func sidecarSourceRaw(forISO iso: String, fileName: String) -> (source: PatchSource, detail: String?)? {
        guard let raw = loadSidecar().games[gameID(forISO: iso)]?[fileName] else { return nil }
        let parts = raw.split(separator: ":", maxSplits: 1).map(String.init)
        guard let head = parts.first, let source = PatchSource(rawValue: head) else { return nil }
        let detail = parts.count > 1 ? parts[1] : nil
        return (source, detail)
    }

    private func sidecarSource(forISO iso: String, fileName: String) -> PatchSource? {
        sidecarSourceRaw(forISO: iso, fileName: fileName)?.source
    }

    private func recordSidecar(forISO iso: String, fileName: String, source: PatchSource, detail: String? = nil) {
        var sidecar = loadSidecar()
        let id = gameID(forISO: iso)
        var perGame = sidecar.games[id] ?? [:]
        if source == .database, let detail, !detail.isEmpty {
            perGame[fileName] = "\(source.rawValue):\(detail)"
        } else {
            perGame[fileName] = source.rawValue
        }
        sidecar.games[id] = perGame
        saveSidecar(sidecar)
    }

    private func clearSidecar(forISO iso: String, fileName: String) {
        var sidecar = loadSidecar()
        let id = gameID(forISO: iso)
        var perGame = sidecar.games[id] ?? [:]
        perGame.removeValue(forKey: fileName)
        if perGame.isEmpty {
            sidecar.games.removeValue(forKey: id)
        } else {
            sidecar.games[id] = perGame
        }
        saveSidecar(sidecar)
    }

    // MARK: - UI messaging

    func dismissMessage() {
        showMessage = false
        lastMessage = nil
    }

    func applyFeedback(_ message: String, kind: PatchFeedbackKind = .information) {
        lastMessage = message
        lastMessageKind = kind
        showMessage = true
    }
}
