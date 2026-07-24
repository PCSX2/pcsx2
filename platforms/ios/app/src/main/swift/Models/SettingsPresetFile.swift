// SettingsPresetFile.swift — Import/export for user settings presets
// SPDX-License-Identifier: GPL-3.0+

import Foundation

struct SettingsPresetImportOutcome: Sendable {
    let name: String
    let appliedFieldCount: Int
}

enum SettingsPresetFileError: LocalizedError {
    case unreadableText
    case unsupportedVersion(String)
    case noSupportedSettings
    case invalidValue(section: String, key: String, value: String)

    var errorDescription: String? {
        switch self {
        case .unreadableText:
            return "The preset is not readable UTF-8 or ASCII text."
        case .unsupportedVersion(let version):
            return "This preset uses unsupported format version \(version)."
        case .noSupportedSettings:
            return "The file does not contain supported ARMSX2 preset settings."
        case .invalidValue(let section, let key, let value):
            return "Invalid value \"\(value)\" for [\(section)] \(key)."
        }
    }
}

enum SettingsPresetFile {
    static let formatVersion = 1

    @MainActor
    static func exportData(
        name: String,
        settings: SettingsStore = .shared
    ) -> Data {
        let safeName = name
            .replacingOccurrences(of: "\r", with: " ")
            .replacingOccurrences(of: "\n", with: " ")
            .trimmingCharacters(in: .whitespaces)
        let effectiveName = safeName.isEmpty ? "ARMSX2 Settings Preset" : safeName
        let text = """
        ; ARMSX2 iOS Settings Preset
        ; Unknown sections and keys are ignored by newer versions.

        [Preset]
        FormatVersion=\(formatVersion)
        Name=\(effectiveName)

        [Emulator]
        FastBoot=\(encode(settings.fastBoot))
        EnablePNACHCheats=\(encode(settings.enableCheats))
        WidescreenPatches=\(encode(settings.enableWidescreenPatches))
        FastCDVD=\(encode(settings.fastCDVD))

        [Graphics]
        FXAA=\(encode(settings.fxaa))
        CASMode=\(settings.casMode)
        CASSharpness=\(settings.casSharpness)
        AspectRatio=\(SettingsStore.aspectRatioName(for: settings.aspectRatio))
        QueueSize=\(settings.vsyncQueueSize)

        [VirtualPad]
        ButtonSkin=\(settings.virtualPadSkin.rawValue)
        """
        return Data(text.utf8)
    }

    @MainActor
    static func importData(
        _ data: Data,
        fallbackName: String,
        settings: SettingsStore = .shared,
        skinLibrary: VPadSkinLibraryStore = .shared
    ) throws -> SettingsPresetImportOutcome {
        guard let text = String(data: data, encoding: .utf8) ??
                String(data: data, encoding: .ascii) else {
            throw SettingsPresetFileError.unreadableText
        }

        let ini = INIValues(text: text)
        if let version = ini.value(section: "Preset", key: "FormatVersion"),
           version != String(formatVersion) {
            throw SettingsPresetFileError.unsupportedVersion(version)
        }

        let values = try ParsedSettings(ini: ini)
        guard values.fieldCount > 0 else {
            throw SettingsPresetFileError.noSupportedSettings
        }
        values.apply(settings: settings, skinLibrary: skinLibrary)

        let declaredName = ini.value(section: "Preset", key: "Name")?
            .trimmingCharacters(in: .whitespacesAndNewlines)
        let name = declaredName?.isEmpty == false ? declaredName! : fallbackName
        return SettingsPresetImportOutcome(name: name, appliedFieldCount: values.fieldCount)
    }

    private static func encode(_ value: Bool) -> String {
        value ? "true" : "false"
    }
}

private struct ParsedSettings {
    var fastBoot: Bool?
    var enableCheats: Bool?
    var widescreenPatches: Bool?
    var fastCDVD: Bool?
    var fxaa: Bool?
    var casMode: Int?
    var casSharpness: Int?
    var aspectRatio: Int?
    var queueSize: Int?
    var buttonSkin: VirtualPadSkin?

    var fieldCount: Int {
        [
            fastBoot != nil,
            enableCheats != nil,
            widescreenPatches != nil,
            fastCDVD != nil,
            fxaa != nil,
            casMode != nil,
            casSharpness != nil,
            aspectRatio != nil,
            queueSize != nil,
            buttonSkin != nil
        ].filter { $0 }.count
    }

    init(ini: INIValues) throws {
        fastBoot = try ini.bool(section: "Emulator", key: "FastBoot")
        enableCheats = try ini.bool(section: "Emulator", key: "EnablePNACHCheats")
        widescreenPatches = try ini.bool(section: "Emulator", key: "WidescreenPatches")
        fastCDVD = try ini.bool(section: "Emulator", key: "FastCDVD")
        fxaa = try ini.bool(section: "Graphics", key: "FXAA")
        casMode = try ini.int(section: "Graphics", key: "CASMode", range: 0...2)
        casSharpness = try ini.int(section: "Graphics", key: "CASSharpness", range: 0...100)
        queueSize = try ini.int(section: "Graphics", key: "QueueSize", range: 2...16)

        if let value = ini.value(section: "Graphics", key: "AspectRatio") {
            let normalized = value.trimmingCharacters(in: .whitespacesAndNewlines)
            switch normalized.lowercased() {
            case "stretch", "0":
                aspectRatio = 0
            case "auto 4:3/3:2", "1":
                aspectRatio = 1
            case "4:3", "2":
                aspectRatio = 2
            case "16:9", "3":
                aspectRatio = 3
            case "10:7", "4":
                aspectRatio = 4
            default:
                throw SettingsPresetFileError.invalidValue(
                    section: "Graphics",
                    key: "AspectRatio",
                    value: value
                )
            }
        }

        if let value = ini.value(section: "VirtualPad", key: "ButtonSkin") {
            let trimmed = value.trimmingCharacters(in: .whitespacesAndNewlines)
            if let rawValue = Int(trimmed), let skin = VirtualPadSkin(rawValue: rawValue) {
                buttonSkin = skin
            } else if let skin = VirtualPadSkin.allCases.first(where: {
                $0.label.caseInsensitiveCompare(trimmed) == .orderedSame
            }) {
                buttonSkin = skin
            } else {
                throw SettingsPresetFileError.invalidValue(
                    section: "VirtualPad",
                    key: "ButtonSkin",
                    value: value
                )
            }
        }
    }

    @MainActor
    func apply(settings: SettingsStore, skinLibrary: VPadSkinLibraryStore) {
        if let fastBoot { settings.fastBoot = fastBoot }
        if let enableCheats { settings.enableCheats = enableCheats }
        if let widescreenPatches { settings.enableWidescreenPatches = widescreenPatches }
        if let fastCDVD { settings.fastCDVD = fastCDVD }
        if let fxaa { settings.fxaa = fxaa }
        if let casMode { settings.casMode = casMode }
        if let casSharpness { settings.casSharpness = casSharpness }
        if let aspectRatio { settings.aspectRatio = aspectRatio }
        if let queueSize { settings.vsyncQueueSize = queueSize }
        if let buttonSkin {
            skinLibrary.selectSkin(id: buttonSkin.descriptorID)
            settings.virtualPadSkin = buttonSkin
        }
    }
}

private struct INIValues {
    private var storage: [String: String] = [:]

    init(text: String) {
        var section = ""
        for rawLine in text.components(separatedBy: .newlines) {
            let line = rawLine.trimmingCharacters(in: .whitespacesAndNewlines)
            guard !line.isEmpty, !line.hasPrefix(";"), !line.hasPrefix("#") else {
                continue
            }
            if line.hasPrefix("["), line.hasSuffix("]") {
                section = String(line.dropFirst().dropLast())
                    .trimmingCharacters(in: .whitespacesAndNewlines)
                continue
            }
            guard let separator = line.firstIndex(of: "=") else { continue }
            let key = String(line[..<separator]).trimmingCharacters(in: .whitespaces)
            let value = String(line[line.index(after: separator)...])
                .trimmingCharacters(in: .whitespaces)
            storage[Self.storageKey(section: section, key: key)] = value
        }
    }

    func value(section: String, key: String) -> String? {
        storage[Self.storageKey(section: section, key: key)]
    }

    func bool(section: String, key: String) throws -> Bool? {
        guard let value = value(section: section, key: key) else { return nil }
        switch value.lowercased() {
        case "true", "yes", "on", "1":
            return true
        case "false", "no", "off", "0":
            return false
        default:
            throw SettingsPresetFileError.invalidValue(section: section, key: key, value: value)
        }
    }

    func int(section: String, key: String, range: ClosedRange<Int>) throws -> Int? {
        guard let value = value(section: section, key: key) else { return nil }
        guard let parsed = Int(value), range.contains(parsed) else {
            throw SettingsPresetFileError.invalidValue(section: section, key: key, value: value)
        }
        return parsed
    }

    private static func storageKey(section: String, key: String) -> String {
        "\(section.lowercased()).\(key.lowercased())"
    }
}
