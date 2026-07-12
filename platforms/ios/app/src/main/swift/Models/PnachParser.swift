// PnachParser.swift — Read-only .pnach text parser for the Cheats & Patches Manager
// SPDX-License-Identifier: GPL-3.0+

import Foundation

enum PnachParseResult {
    case valid(entries: [ParsedPatchEntry])
    case invalid(reason: String)
}

struct ParsedPatchEntry: Identifiable {
    let id: String
    let name: String        // empty for legacy/unlabeled groups
    let summary: String
    let category: PatchCategory
    let isLegacy: Bool
}

/// A header-delimited region of a .pnach file, with its raw line range. `draftIndex` mirrors
/// the `entry-N` / `legacy-N` suffix used in `ParsedPatchEntry.id`, so a block can be matched
/// back to a `PatchEntry` shown in the manager for safe single-entry deletion.
struct PnachBlock {
    let name: String        // header name; "" for the unnamed preamble block
    let isLegacy: Bool      // true when the block has no header name
    let hasPatch: Bool      // block contains at least one patch=/dpatch= line
    let draftIndex: Int     // order among hasPatch blocks (-1 when !hasPatch)
    let startLine: Int      // inclusive index into the normalized line array
    let endLine: Int        // exclusive index into the normalized line array
}

enum PnachParser {
    private struct Draft {
        var name = ""
        var description = ""
        var author = ""
        var comment = ""
        var hasPatch = false
    }

    static func parse(_ text: String) -> PnachParseResult {
        let lines = text
            .replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")
            .components(separatedBy: "\n")

        var sawPatch = false
        var drafts: [Draft] = []
        var current = Draft()

        for raw in lines {
            var line = raw
            line = Self.stripInlineComment(line)
            line = line.trimmingCharacters(in: .whitespaces)
            if line.isEmpty { continue }

            if line.hasPrefix("[") && line.hasSuffix("]") && line.count > 2 {
                if current.hasPatch { drafts.append(current) }
                current = Draft()
                current.name = String(line.dropFirst().dropLast()).trimmingCharacters(in: .whitespaces)
                continue
            }

            guard let equals = line.range(of: "=") else { continue }
            let key = line[..<equals.lowerBound].trimmingCharacters(in: .whitespaces).lowercased()
            let value = String(line[equals.upperBound...]).trimmingCharacters(in: .whitespaces)
            switch key {
            case "patch", "dpatch":
                sawPatch = true
                current.hasPatch = true
            case "description": current.description = value
            case "author": current.author = value
            case "comment": current.comment = value
            default: break
            }
        }

        if current.hasPatch { drafts.append(current) }

        guard sawPatch else {
            return .invalid(reason: "No usable patch entries were found.")
        }

        var entries: [ParsedPatchEntry] = []
        for (index, draft) in drafts.enumerated() {
            let isLegacy = draft.name.isEmpty
            entries.append(ParsedPatchEntry(
                id: isLegacy ? "legacy-\(index)" : "entry-\(index)",
                name: draft.name,
                summary: Self.summary(for: draft),
                category: Self.category(for: draft, isLegacy: isLegacy),
                isLegacy: isLegacy
            ))
        }

        return .valid(entries: entries)
    }

    private static func summary(for draft: Draft) -> String {
        var parts: [String] = []
        let description = draft.description.trimmingCharacters(in: .whitespacesAndNewlines)
        if !description.isEmpty { parts.append(description) }
        let author = draft.author.trimmingCharacters(in: .whitespacesAndNewlines)
        if !author.isEmpty { parts.append("Author: \(author)") }
        let comment = draft.comment.trimmingCharacters(in: .whitespacesAndNewlines)
        if !comment.isEmpty && comment != description { parts.append(comment) }
        return parts.joined(separator: " — ")
    }

    private static func category(for draft: Draft, isLegacy: Bool) -> PatchCategory {
        let name = draft.name.lowercased()
        let blob = (draft.name + " " + draft.description + " " + draft.comment).lowercased()

        // Cheat/hack keywords win first so a presentation word in the description can't
        // reclassify a gameplay cheat (e.g. "60fps invincible health" stays .cheats).
        let cheatWords = ["cheat", "hack", "infinite", "infinity", "unlimited", "max money",
                          "max ammo", "infinite health", "god mode", "one-hit", "one hit kill",
                          "no death", "never die", "unlock all", "infinite mp", "max hp"]
        if cheatWords.contains(where: { blob.contains($0) }) && !isLegacy { return .cheats }

        // Prefer the header name for presentation categories — it's the more structured field.
        if name.contains("widescreen") || name.contains("16:9") { return .widescreen }
        if name.contains("60fps") || name.contains("60 fps") || name.contains("60-frame") { return .fps60 }
        if blob.contains("widescreen") || blob.contains("16:9") { return .widescreen }
        if blob.contains("60fps") || blob.contains("60 fps") || blob.contains("60-frame") { return .fps60 }
        if blob.contains("no-interlace") || blob.contains("no interlace") || blob.contains("deinterlace") || blob.contains("progressive") { return .noInterlace }
        if blob.contains("performance") || blob.contains("fastboot") || blob.contains("fast boot") { return .performance }
        if blob.contains("compatib") || name.contains("fix") { return .gameFix }
        if blob.contains("experimental") || blob.contains("beta") { return .experimental }
        return isLegacy ? .other : .cheats
    }

    // MARK: - Block model (for single-entry deletion)

    /// Normalizes line endings and splits into lines, matching how `parse` sees the text.
    private static func normalizedLines(_ text: String) -> [String] {
        text
            .replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")
            .components(separatedBy: "\n")
    }

    /// Strips `//` comments and surrounding whitespace, matching `parse`'s line handling.
    private static func cleanedLine(_ line: String) -> String {
        stripInlineComment(line).trimmingCharacters(in: .whitespaces)
    }

    /// Strips an inline // comment unless it's part of a URL scheme (http://).
    private static func stripInlineComment(_ line: String) -> String {
        var searchStart = line.startIndex
        while let slashRange = line.range(of: "//", range: searchStart..<line.endIndex) {
            let pos = slashRange.lowerBound
            let precededByWhitespace = pos == line.startIndex
                || line[line.index(before: pos)].isWhitespace
            if precededByWhitespace {
                return String(line[..<pos])
            }
            // part of http://; keep scanning
            searchStart = slashRange.upperBound
        }
        return line
    }

    private static func isHeader(_ cleaned: String) -> Bool {
        cleaned.hasPrefix("[") && cleaned.hasSuffix("]") && cleaned.count > 2
    }

    private static func headerName(_ cleaned: String) -> String {
        String(cleaned.dropFirst().dropLast()).trimmingCharacters(in: .whitespaces)
    }

    /// Splits the normalized line array into header-delimited blocks. `draftIndex` is assigned
    /// in file order, counting only blocks that contain a patch, exactly like `parse` builds its
    /// drafts — so it matches the `entry-N` / `legacy-N` id suffix.
    private static func buildBlocks(_ lines: [String]) -> [PnachBlock] {
        var boundaries = [0]
        for (index, line) in lines.enumerated() where index > 0 && isHeader(cleanedLine(line)) {
            boundaries.append(index)
        }

        var blocks: [PnachBlock] = []
        var draftIndex = -1
        for (position, start) in boundaries.enumerated() {
            let end = position + 1 < boundaries.count ? boundaries[position + 1] : lines.count
            var name = ""
            if start < lines.count, isHeader(cleanedLine(lines[start])) {
                name = headerName(cleanedLine(lines[start]))
            }

            var hasPatch = false
            for lineIndex in start..<end where lineIndex < lines.count {
                let cleaned = cleanedLine(lines[lineIndex])
                guard let equals = cleaned.range(of: "=") else { continue }
                let key = cleaned[..<equals.lowerBound]
                    .trimmingCharacters(in: .whitespaces)
                    .lowercased()
                if key == "patch" || key == "dpatch" {
                    hasPatch = true
                    break
                }
            }

            if hasPatch { draftIndex += 1 }
            blocks.append(PnachBlock(
                name: name,
                isLegacy: name.isEmpty,
                hasPatch: hasPatch,
                draftIndex: hasPatch ? draftIndex : -1,
                startLine: start,
                endLine: end
            ))
        }
        return blocks
    }

    /// Header-delimited blocks with their raw line ranges.
    static func blocks(_ text: String) -> [PnachBlock] {
        buildBlocks(normalizedLines(text))
    }

    /// Returns the text with the draft block at `draftIndex` removed, or nil if no such block
    /// exists. Removes only that entry's lines so the rest of the file is preserved.
    static func removingDraftBlock(_ text: String, draftIndex target: Int) -> String? {
        let lines = normalizedLines(text)
        guard let block = buildBlocks(lines).first(where: { $0.hasPatch && $0.draftIndex == target }) else {
            return nil
        }
        var kept = lines
        kept.removeSubrange(block.startLine..<block.endLine)
        var result = kept.joined(separator: "\n")
        if !result.isEmpty, !result.hasSuffix("\n") { result.append("\n") }
        return result
    }

    /// Combines `existing` and `new` .pnach text. Named blocks present in both are taken from
    /// `new` (the existing copy is dropped first), so re-importing or re-downloading a patch
    /// updates it instead of creating a duplicate; blocks unique to either side are preserved.
    /// Unnamed (legacy) blocks are never matched and are kept/appended as-is.
    static func merging(existing: String, new: String) -> String {
        let existingLines = normalizedLines(existing)
        let existingBlocks = buildBlocks(existingLines)
        let replacingNames = Set(
            blocks(new)
                .filter { !$0.isLegacy && !$0.name.isEmpty }
                .map { $0.name.lowercased() }
        )

        var kept = existingLines
        for block in existingBlocks
            .filter({ !$0.isLegacy && !$0.name.isEmpty && replacingNames.contains($0.name.lowercased()) })
            .sorted(by: { $0.startLine > $1.startLine }) {
            kept.removeSubrange(block.startLine..<block.endLine)
        }

        var result = kept.joined(separator: "\n")
        let newNormalized = new
            .replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")

        if result.isEmpty {
            result = newNormalized
        } else {
            if !result.hasSuffix("\n") { result.append("\n") }
            result += newNormalized
        }
        if !result.isEmpty, !result.hasSuffix("\n") { result.append("\n") }
        return result
    }

    /// Wraps headerless (legacy) `patch=`/`dpatch=` lines in a named `[header]` section so they
    /// survive `merging` as a distinct, individually-manageable entry. Without a header, unnamed
    /// patch lines are absorbed into whichever named section precedes them during parsing, so an
    /// imported custom `.pnach` can vanish from the manager list and can't be toggled or removed
    /// on its own. Only leading headerless patch lines (before the first `[section]`) are wrapped;
    /// once a named section starts its own lines are left intact.
    static func wrappingLegacyPatches(_ text: String, header: String) -> String {
        let lines = normalizedLines(text)
        var output: [String] = []
        var openedWrapper = false
        var reachedNamedSection = false
        for line in lines {
            let cleaned = cleanedLine(line)
            if isHeader(cleaned) {
                reachedNamedSection = true
                output.append(line)
                continue
            }
            if !reachedNamedSection && !openedWrapper {
                if let equals = cleaned.range(of: "=") {
                    let key = cleaned[..<equals.lowerBound].trimmingCharacters(in: .whitespaces).lowercased()
                    if key == "patch" || key == "dpatch" {
                        output.append("[\(header)]")
                        openedWrapper = true
                    }
                }
            }
            output.append(line)
        }

        guard openedWrapper else { return text }
        var result = output.joined(separator: "\n")
        if !result.isEmpty, !result.hasSuffix("\n") { result.append("\n") }
        return result
    }
}
