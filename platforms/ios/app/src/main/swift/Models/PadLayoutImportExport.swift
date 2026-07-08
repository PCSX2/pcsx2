// SPDX-License-Identifier: GPL-3.0+

import Foundation
import SwiftUI

struct PadLayoutImportResult {
    var displayName: String?
    var snapshot: PadLayoutSnapshot
}

enum PadLayoutImportExport {
    static let schemaVersion = 1

    static func exportData(for preset: PadLayoutPreset) throws -> Data {
        try exportData(displayName: preset.displayName, snapshot: preset.snapshot)
    }

    static func exportData(displayName: String, snapshot: PadLayoutSnapshot) throws -> Data {
        let payload = PadLayoutExportPayload(displayName: displayName, snapshot: snapshot)
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        return try encoder.encode(payload)
    }

    static func decodeImport(from data: Data, fallbackName: String?) throws -> PadLayoutImportResult {
        let decoder = JSONDecoder()
        let payload = try decoder.decode(PadLayoutExportPayload.self, from: data)
        return PadLayoutImportResult(
            displayName: sanitizedDisplayName(
                payload.displayName ?? payload.name,
                fallback: sourceName(from: fallbackName)
            ),
            snapshot: clampedSnapshot(payload.snapshot)
        )
    }

    static func decodeSnapshot(from data: Data) throws -> PadLayoutSnapshot {
        try decodeImport(from: data, fallbackName: nil).snapshot
    }

    static func exportedFileName(for displayName: String) -> String {
        let name = sanitizedFileStem(displayName, fallback: "Layout")
        return "\(name).layout.json"
    }

    static func clampedSnapshot(_ snapshot: PadLayoutSnapshot) -> PadLayoutSnapshot {
        PadLayoutSnapshot(
            portrait: clampPositions(snapshot.portrait),
            landscape: clampPositions(snapshot.landscape),
            perButtonPortrait: clampPositions(snapshot.perButtonPortrait),
            perButtonLandscape: clampPositions(snapshot.perButtonLandscape),
            controlVisibility: snapshot.controlVisibility
        )
    }

    private static func clampPositions(_ positions: [String: PadGroupPosition]) -> [String: PadGroupPosition] {
        positions.mapValues { position in
            PadGroupPosition(
                x: clampedUnit(position.x),
                y: clampedUnit(position.y),
                scaleX: PadLayoutMetrics.clampedScale(position.scaleX),
                scaleY: PadLayoutMetrics.clampedScale(position.scaleY),
                hitScaleX: PadLayoutMetrics.clampedScale(position.hitScaleX),
                hitScaleY: PadLayoutMetrics.clampedScale(position.hitScaleY)
            )
        }
    }

    private static func clampedUnit(_ value: CGFloat) -> CGFloat {
        guard value.isFinite else { return 0.5 }
        return min(max(value, 0), 1)
    }

    private static func sanitizedDisplayName(_ name: String?, fallback: String?) -> String? {
        let trimmed = (name ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        if !trimmed.isEmpty {
            return trimmed
        }
        let fallback = (fallback ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        return fallback.isEmpty ? nil : fallback
    }

    private static func sourceName(from fileName: String?) -> String? {
        guard let fileName else { return nil }
        var stem = URL(fileURLWithPath: fileName).deletingPathExtension().lastPathComponent
        if stem.lowercased().hasSuffix(".layout") {
            stem = String(stem.dropLast(".layout".count))
        }
        return stem.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    private static func sanitizedFileStem(_ name: String, fallback: String) -> String {
        let invalid = CharacterSet(charactersIn: "\\/:*?\"<>|")
            .union(.newlines)
            .union(.controlCharacters)
        let cleaned = name
            .components(separatedBy: invalid)
            .joined(separator: " ")
            .trimmingCharacters(in: .whitespacesAndNewlines)
        return cleaned.isEmpty ? fallback : cleaned
    }
}

private struct PadLayoutExportPayload: Codable {
    var schemaVersion: Int?
    var displayName: String?
    var name: String?
    var portrait: [String: PadGroupPosition]
    var landscape: [String: PadGroupPosition]
    var perButtonPortrait: [String: PadGroupPosition]
    var perButtonLandscape: [String: PadGroupPosition]
    var controlVisibility: [String: Bool]

    var snapshot: PadLayoutSnapshot {
        PadLayoutSnapshot(
            portrait: portrait,
            landscape: landscape,
            perButtonPortrait: perButtonPortrait,
            perButtonLandscape: perButtonLandscape,
            controlVisibility: controlVisibility
        )
    }

    init(displayName: String, snapshot: PadLayoutSnapshot) {
        self.schemaVersion = PadLayoutImportExport.schemaVersion
        self.displayName = displayName
        self.name = displayName
        self.portrait = snapshot.portrait
        self.landscape = snapshot.landscape
        self.perButtonPortrait = snapshot.perButtonPortrait
        self.perButtonLandscape = snapshot.perButtonLandscape
        self.controlVisibility = snapshot.controlVisibility
    }
}
