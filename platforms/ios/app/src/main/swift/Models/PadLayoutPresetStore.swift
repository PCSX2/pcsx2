// PadLayoutPresetStore.swift - JSON-backed virtual pad layout presets
// SPDX-License-Identifier: GPL-3.0+

import Foundation
import SwiftUI

struct PadLayoutSnapshot: Codable, Equatable {
    var portrait: [String: PadGroupPosition]
    var landscape: [String: PadGroupPosition]
    var perButtonPortrait: [String: PadGroupPosition]
    var perButtonLandscape: [String: PadGroupPosition]
    var controlVisibility: [String: Bool]

    static let builtInDefault = PadLayoutSnapshot(
        portrait: PadLayoutStore.defaultPortrait,
        landscape: PadLayoutStore.defaultLandscape,
        perButtonPortrait: [:],
        perButtonLandscape: [:],
        controlVisibility: [:]
    )

    func position(for id: String, landscape isLandscape: Bool) -> PadGroupPosition {
        let dict = isLandscape ? landscape : portrait
        let defaults = isLandscape ? PadLayoutStore.defaultLandscape : PadLayoutStore.defaultPortrait
        return dict[id] ?? defaults[id] ?? PadGroupPosition(x: 0.5, y: 0.5, scale: 1.0)
    }

    func perButtonPosition(for id: String, landscape isLandscape: Bool, areaW: CGFloat, areaH: CGFloat) -> PadGroupPosition {
        let dict = isLandscape ? perButtonLandscape : perButtonPortrait
        if let pos = dict[id] {
            return pos
        }

        let groupID = PadLayoutStore.groupID(for: id)
        let groupPos = position(for: groupID, landscape: isLandscape)
        let offset = VirtualPadButtonOffset.offset(for: id, isLandscape: isLandscape)
        let scaledOffsetX = offset.width * groupPos.scaleX
        let scaledOffsetY = offset.height * groupPos.scaleY
        return PadGroupPosition(
            x: groupPos.x + scaledOffsetX / areaW,
            y: groupPos.y + scaledOffsetY / areaH,
            scaleX: groupPos.scaleX,
            scaleY: groupPos.scaleY,
            hitScaleX: groupPos.hitScaleX,
            hitScaleY: groupPos.hitScaleY
        )
    }

    func isControlVisible(_ id: String) -> Bool {
        if let explicit = controlVisibility[id] {
            return explicit
        }
        let group = PadLayoutStore.groupID(for: id)
        return controlVisibility[group] ?? true
    }
}

enum PadLayoutPresetSource: String, Codable, Equatable {
    case user
    case builtIn
    case futureImportedSkin
}

struct PadLayoutPreset: Codable, Equatable, Identifiable {
    var id: String
    var displayName: String
    var createdAt: Date
    var updatedAt: Date
    var source: PadLayoutPresetSource
    var linkedSkinID: String?
    var snapshot: PadLayoutSnapshot
}

struct PadLayoutEditorContext: Equatable {
    var presetID: String?
    var gameIdentity: PadLayoutGameIdentity?
    var initialSnapshot: PadLayoutSnapshot?
    var skinDescriptor: VPadSkinDescriptor?

    init(
        presetID: String? = nil,
        gameIdentity: PadLayoutGameIdentity? = nil,
        initialSnapshot: PadLayoutSnapshot? = nil,
        skinDescriptor: VPadSkinDescriptor? = nil
    ) {
        self.presetID = presetID
        self.gameIdentity = gameIdentity
        self.initialSnapshot = initialSnapshot
        self.skinDescriptor = skinDescriptor
    }

    static let current = PadLayoutEditorContext(
        presetID: nil,
        gameIdentity: nil,
        initialSnapshot: nil,
        skinDescriptor: nil
    )
}

struct PadLayoutGameIdentity: Codable, Equatable, Hashable, Identifiable {
    let serial: String
    let crc: String

    var id: String {
        "\(serial)|\(crc)"
    }

    init?(serial: String?, crc: String?) {
        let normalizedSerial = Self.normalizedSerial(serial)
        let normalizedCRC = Self.normalizedCRC(crc)
        guard !normalizedSerial.isEmpty, !normalizedCRC.isEmpty else {
            return nil
        }
        self.serial = normalizedSerial
        self.crc = normalizedCRC
    }

    init(serial: String, crc: String) {
        self.serial = Self.normalizedSerial(serial)
        self.crc = Self.normalizedCRC(crc)
    }

    static func normalizedSerial(_ value: String?) -> String {
        (value ?? "")
            .replacingOccurrences(of: "_", with: "-")
            .trimmingCharacters(in: .whitespacesAndNewlines)
            .uppercased()
    }

    static func normalizedCRC(_ value: String?) -> String {
        var raw = (value ?? "")
            .trimmingCharacters(in: .whitespacesAndNewlines)
            .uppercased()
        if raw.hasPrefix("CRC-") {
            raw.removeFirst(4)
        }
        raw = raw.replacingOccurrences(of: "0X", with: "")
        raw = String(raw.filter { character in
            ("0"..."9").contains(character) || ("A"..."F").contains(character)
        })
        guard !raw.isEmpty, (UInt64(raw, radix: 16) ?? 0) != 0 else {
            return ""
        }
        return raw
    }
}

struct VPadGameAssignment: Codable, Equatable {
    var layoutPresetID: String?
    var skinID: String?

    init(layoutPresetID: String? = nil, skinID: String? = nil) {
        self.layoutPresetID = layoutPresetID
        self.skinID = skinID
    }

    var isEmpty: Bool {
        layoutPresetID == nil && skinID == nil
    }
}

private struct PadLayoutPresetLibrary: Codable {
    var schemaVersion: Int
    var presets: [PadLayoutPreset]
    var globalPresetID: String?
    var gameAssignments: [String: VPadGameAssignment]

    private enum CodingKeys: String, CodingKey {
        case schemaVersion
        case presets
        case globalPresetID
        case gameAssignments
    }

    init(
        schemaVersion: Int,
        presets: [PadLayoutPreset],
        globalPresetID: String?,
        gameAssignments: [String: VPadGameAssignment]
    ) {
        self.schemaVersion = schemaVersion
        self.presets = presets
        self.globalPresetID = globalPresetID
        self.gameAssignments = gameAssignments
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        schemaVersion = try container.decode(Int.self, forKey: .schemaVersion)
        presets = try container.decode([PadLayoutPreset].self, forKey: .presets)
        globalPresetID = try container.decodeIfPresent(String.self, forKey: .globalPresetID)
        if let assignments = try? container.decode([String: VPadGameAssignment].self, forKey: .gameAssignments) {
            gameAssignments = assignments
        } else {
            let oldAssignments = try container.decodeIfPresent([String: String].self, forKey: .gameAssignments) ?? [:]
            gameAssignments = oldAssignments.mapValues { VPadGameAssignment(layoutPresetID: $0, skinID: nil) }
        }
    }
}

@Observable
final class PadLayoutPresetStore: @unchecked Sendable {
    static let shared = PadLayoutPresetStore()
    static let schemaVersion = 2

    private let libraryURL: URL
    private(set) var presets: [PadLayoutPreset] = []
    var globalPresetID: String? {
        didSet {
            if globalPresetID != oldValue {
                persist()
            }
        }
    }
    private var gameAssignments: [String: VPadGameAssignment] = [:]

    init(
        libraryURL: URL? = nil,
        migrateFromCurrentLayout: Bool = true,
        currentLayout: PadLayoutStore = .shared
    ) {
        self.libraryURL = libraryURL ?? Self.defaultLibraryURL()
        let loadResult = loadLibrary()
        switch loadResult {
        case .loaded(let library):
            presets = library.presets
            globalPresetID = validPresetID(library.globalPresetID)
            gameAssignments = sanitizedAssignments(library.gameAssignments)
        case .missing:
            if migrateFromCurrentLayout {
                migrateCurrentLayout(currentLayout)
            }
        case .corrupt:
            presets = []
            globalPresetID = nil
            gameAssignments = [:]
        }
    }

    @discardableResult
    func createPreset(
        named name: String,
        snapshot: PadLayoutSnapshot,
        source: PadLayoutPresetSource = .user,
        linkedSkinID: String? = nil
    ) -> PadLayoutPreset {
        let now = Date()
        let preset = PadLayoutPreset(
            id: UUID().uuidString,
            displayName: sanitizedName(name, fallback: "Layout"),
            createdAt: now,
            updatedAt: now,
            source: source,
            linkedSkinID: linkedSkinID,
            snapshot: snapshot
        )
        presets.append(preset)
        persist()
        return preset
    }

    @discardableResult
    func importLayout(data: Data, fallbackName: String?) throws -> PadLayoutPreset {
        let imported = try PadLayoutImportExport.decodeImport(from: data, fallbackName: fallbackName)
        return createPreset(
            named: uniqueDisplayName(imported.displayName ?? "Imported Layout"),
            snapshot: imported.snapshot
        )
    }

    func updatePreset(id: String, snapshot: PadLayoutSnapshot) throws {
        guard let index = presets.firstIndex(where: { $0.id == id }) else {
            throw PadLayoutPresetStoreError.missingPreset
        }
        presets[index].snapshot = snapshot
        presets[index].updatedAt = Date()
        persist()
    }

    func renamePreset(id: String, to name: String) throws {
        guard let index = presets.firstIndex(where: { $0.id == id }) else {
            throw PadLayoutPresetStoreError.missingPreset
        }
        presets[index].displayName = sanitizedName(name, fallback: presets[index].displayName)
        presets[index].updatedAt = Date()
        persist()
    }

    @discardableResult
    func duplicatePreset(id: String, named name: String? = nil) throws -> PadLayoutPreset {
        guard let original = presets.first(where: { $0.id == id }) else {
            throw PadLayoutPresetStoreError.missingPreset
        }
        return createPreset(
            named: name ?? "\(original.displayName) Copy",
            snapshot: original.snapshot,
            source: .user,
            linkedSkinID: original.linkedSkinID
        )
    }

    func deletePreset(id: String) throws {
        guard presets.contains(where: { $0.id == id }) else {
            throw PadLayoutPresetStoreError.missingPreset
        }
        presets.removeAll { $0.id == id }
        if globalPresetID == id {
            globalPresetID = nil
        }
        gameAssignments = gameAssignments.compactMapValues { assignment in
            var updated = assignment
            if updated.layoutPresetID == id {
                updated.layoutPresetID = nil
            }
            return updated.isEmpty ? nil : updated
        }
        persist()
    }

    func preset(id: String?) -> PadLayoutPreset? {
        guard let id else { return nil }
        return presets.first { $0.id == id }
    }

    func presetID(for identity: PadLayoutGameIdentity) -> String? {
        validPresetID(gameAssignments[identity.id]?.layoutPresetID)
    }

    func setPreset(_ presetID: String?, for identity: PadLayoutGameIdentity) {
        var assignment = gameAssignments[identity.id] ?? VPadGameAssignment()
        if let presetID, validPresetID(presetID) != nil {
            assignment.layoutPresetID = presetID
        } else {
            assignment.layoutPresetID = nil
        }
        setAssignment(assignment, for: identity)
        persist()
    }

    func skinID(for identity: PadLayoutGameIdentity) -> String? {
        gameAssignments[identity.id]?.skinID
    }

    func setSkin(_ skinID: String?, for identity: PadLayoutGameIdentity) {
        var assignment = gameAssignments[identity.id] ?? VPadGameAssignment()
        assignment.skinID = skinID?.trimmingCharacters(in: .whitespacesAndNewlines).nilIfEmpty
        setAssignment(assignment, for: identity)
        persist()
    }

    func setSkin(_ skinID: String?, for identity: PadLayoutGameIdentity, using skinLibrary: VPadSkinLibraryStore) {
        if let skinID, skinLibrary.descriptor(id: skinID) != nil {
            setSkin(skinID, for: identity)
        } else {
            clearSkin(for: identity)
        }
    }

    func clearSkin(for identity: PadLayoutGameIdentity) {
        var assignment = gameAssignments[identity.id] ?? VPadGameAssignment()
        assignment.skinID = nil
        setAssignment(assignment, for: identity)
        persist()
    }

    func clearSkinAssignments(forSkinID skinID: String) {
        gameAssignments = gameAssignments.compactMapValues { assignment in
            var updated = assignment
            if updated.skinID == skinID {
                updated.skinID = nil
            }
            return updated.isEmpty ? nil : updated
        }
        persist()
    }

    func clearVPadOverrides(for identity: PadLayoutGameIdentity) {
        gameAssignments.removeValue(forKey: identity.id)
        persist()
    }

    func effectivePreset(for identity: PadLayoutGameIdentity?) -> PadLayoutPreset? {
        if let identity,
           let assignedID = presetID(for: identity),
           let preset = preset(id: assignedID) {
            return preset
        }
        return preset(id: globalPresetID)
    }

    func effectiveSnapshot(for identity: PadLayoutGameIdentity?) -> PadLayoutSnapshot? {
        effectivePreset(for: identity)?.snapshot
    }

    func effectiveSkinDescriptor(for identity: PadLayoutGameIdentity?, using skinLibrary: VPadSkinLibraryStore) -> VPadSkinDescriptor {
        if let identity,
           let assignedID = skinID(for: identity),
           let descriptor = skinLibrary.descriptor(id: assignedID) {
            return descriptor
        }
        return skinLibrary.selectedDescriptor
    }

    func save() throws {
        let library = PadLayoutPresetLibrary(
            schemaVersion: Self.schemaVersion,
            presets: presets,
            globalPresetID: validPresetID(globalPresetID),
            gameAssignments: sanitizedAssignments(gameAssignments)
        )
        try FileManager.default.createDirectory(
            at: libraryURL.deletingLastPathComponent(),
            withIntermediateDirectories: true
        )
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        encoder.dateEncodingStrategy = .iso8601
        try encoder.encode(library).write(to: libraryURL, options: .atomic)
    }

    private enum LoadResult {
        case loaded(PadLayoutPresetLibrary)
        case missing
        case corrupt
    }

    private func loadLibrary() -> LoadResult {
        guard FileManager.default.fileExists(atPath: libraryURL.path) else {
            return .missing
        }
        do {
            let decoder = JSONDecoder()
            decoder.dateDecodingStrategy = .iso8601
            let data = try Data(contentsOf: libraryURL)
            return .loaded(try decoder.decode(PadLayoutPresetLibrary.self, from: data))
        } catch {
            return .corrupt
        }
    }

    private func migrateCurrentLayout(_ currentLayout: PadLayoutStore) {
        let preset = createPreset(named: "Current Layout", snapshot: currentLayout.snapshot())
        globalPresetID = preset.id
        persist()
    }

    private func validPresetID(_ id: String?) -> String? {
        guard let id, presets.contains(where: { $0.id == id }) else {
            return nil
        }
        return id
    }

    private func sanitizedAssignments(_ assignments: [String: VPadGameAssignment]) -> [String: VPadGameAssignment] {
        assignments.compactMapValues { assignment in
            let sanitized = VPadGameAssignment(
                layoutPresetID: validPresetID(assignment.layoutPresetID),
                skinID: assignment.skinID?.trimmingCharacters(in: .whitespacesAndNewlines).nilIfEmpty
            )
            return sanitized.isEmpty ? nil : sanitized
        }
    }

    private func setAssignment(_ assignment: VPadGameAssignment, for identity: PadLayoutGameIdentity) {
        if assignment.isEmpty {
            gameAssignments.removeValue(forKey: identity.id)
        } else {
            gameAssignments[identity.id] = assignment
        }
    }

    private func sanitizedName(_ name: String, fallback: String) -> String {
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        return trimmed.isEmpty ? fallback : trimmed
    }

    private func uniqueDisplayName(_ name: String) -> String {
        let base = sanitizedName(name, fallback: "Imported Layout")
        let used = Set(presets.map { $0.displayName.lowercased() })
        if !used.contains(base.lowercased()) {
            return base
        }
        var suffix = 2
        while used.contains("\(base) \(suffix)".lowercased()) {
            suffix += 1
        }
        return "\(base) \(suffix)"
    }

    private func persist() {
        do {
            try save()
        } catch {
            NSLog("[ARMSX2 iOS VPad] Failed to save layout presets: %@", error.localizedDescription)
        }
    }

    private static func defaultLibraryURL() -> URL {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first
            ?? FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first
            ?? FileManager.default.temporaryDirectory
        return base
            .appendingPathComponent("ARMSX2", isDirectory: true)
            .appendingPathComponent("VPadLayouts.json")
    }
}

enum PadLayoutPresetStoreError: Error {
    case missingPreset
}

private extension String {
    var nilIfEmpty: String? {
        isEmpty ? nil : self
    }
}
