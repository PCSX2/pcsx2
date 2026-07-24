// SettingsPresetsView.swift — Cross-category settings presets
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UniformTypeIdentifiers
import UIKit

private extension UTType {
    static let armsx2SettingsPreset = UTType(
        exportedAs: "com.armsx2.settings-preset",
        conformingTo: .plainText
    )
}

private struct SettingsPresetDocument: FileDocument {
    static var readableContentTypes: [UTType] { [.armsx2SettingsPreset, .plainText] }

    var data: Data

    init(data: Data) {
        self.data = data
    }

    init(configuration: ReadConfiguration) throws {
        data = configuration.file.regularFileContents ?? Data()
    }

    func fileWrapper(configuration: WriteConfiguration) throws -> FileWrapper {
        FileWrapper(regularFileWithContents: data)
    }
}

private enum SettingsPresetsSheet: String, Identifiable {
    case folderPicker
    case presetImporter

    var id: String { rawValue }
}

private struct SettingsPresetsMessage: Identifiable {
    let id = UUID()
    let title: String
    let text: String
}

/// Plain system folder picker. The selected URL is forwarded unchanged so its
/// provider-granted access token remains attached.
private struct ARMSX2FolderPicker: UIViewControllerRepresentable {
    let onComplete: (Result<URL, Error>) -> Void

    func makeCoordinator() -> Coordinator {
        Coordinator(onComplete: onComplete)
    }

    func makeUIViewController(context: Context) -> UIDocumentPickerViewController {
        let picker = UIDocumentPickerViewController(forOpeningContentTypes: [.folder])
        picker.allowsMultipleSelection = false
        picker.delegate = context.coordinator
        return picker
    }

    func updateUIViewController(
        _ uiViewController: UIDocumentPickerViewController,
        context: Context
    ) {}

    final class Coordinator: NSObject, UIDocumentPickerDelegate {
        private let onComplete: (Result<URL, Error>) -> Void

        init(onComplete: @escaping (Result<URL, Error>) -> Void) {
            self.onComplete = onComplete
        }

        func documentPicker(
            _ controller: UIDocumentPickerViewController,
            didPickDocumentsAt urls: [URL]
        ) {
            guard let url = urls.first else {
                onComplete(.failure(CocoaError(.fileNoSuchFile)))
                return
            }
            onComplete(.success(url))
        }

        func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
            onComplete(.failure(CocoaError(.userCancelled)))
        }
    }
}

struct SettingsPresetsView: View {
    @State private var settings = SettingsStore.shared
    @State private var skinLibrary = VPadSkinLibraryStore.shared
    @State private var folderAccess = InitialContentBootstrap.shared
    @State private var presentedSheet: SettingsPresetsSheet?
    @State private var message: SettingsPresetsMessage?
    @State private var exportDocument = SettingsPresetDocument(data: Data())
    @State private var isExportingPreset = false

    var body: some View {
        Form {
            folderAccessSection
            devicePresetsSection
            presetFilesSection
        }
        .navigationTitle(settings.localized("Settings Presets"))
        .navigationBarTitleDisplayMode(.inline)
        .sheet(item: $presentedSheet) { sheet in
            switch sheet {
            case .folderPicker:
                ARMSX2FolderPicker { result in
                    handleFolderPickerResult(result)
                }
            case .presetImporter:
                ImportDocumentPicker(
                    allowedContentTypes: presetImportContentTypes,
                    allowsMultipleSelection: false,
                    asCopy: true
                ) { result in
                    handlePresetPickerResult(result)
                }
            }
        }
        .fileExporter(
            isPresented: $isExportingPreset,
            document: exportDocument,
            contentType: .armsx2SettingsPreset,
            defaultFilename: "ARMSX2 Custom Preset.ini"
        ) { result in
            switch result {
            case .success:
                message = SettingsPresetsMessage(
                    title: "Preset Exported",
                    text: "The current preset settings were exported as an .ini file."
                )
            case .failure(let error):
                if !FileImportHandler.isUserCancelledPickerError(error) {
                    message = SettingsPresetsMessage(
                        title: "Preset Export Failed",
                        text: error.localizedDescription
                    )
                }
            }
        }
        .alert(item: $message) { message in
            Alert(
                title: Text(settings.localized(message.title)),
                message: Text(settings.localized(message.text)),
                dismissButton: .default(Text(settings.localized("OK")))
            )
        }
    }

    private var folderAccessSection: some View {
        Section {
            HStack {
                Label(
                    settings.localized("ARMSX2 Folder"),
                    systemImage: folderAccess.hasSelectedFolder ? "folder.fill.badge.checkmark" : "folder"
                )
                Spacer()
                Text(folderAccess.selectedFolderName ?? settings.localized("Not Selected"))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                    .lineLimit(1)
            }

            Button {
                presentedSheet = .folderPicker
            } label: {
                Label(
                    settings.localized(folderAccess.hasSelectedFolder
                        ? "Change ARMSX2 Folder"
                        : "Select ARMSX2 Folder"),
                    systemImage: "folder.badge.plus"
                )
            }

            if folderAccess.hasSelectedFolder {
                Button {
                    scanSelectedFolder()
                } label: {
                    HStack {
                        Label(settings.localized("Scan Selected Folder"), systemImage: "arrow.clockwise")
                        Spacer()
                        if folderAccess.isRunning {
                            ProgressView()
                        }
                    }
                }
                .disabled(folderAccess.isRunning)

                Button(role: .destructive) {
                    folderAccess.removeSelectedFolder()
                    message = SettingsPresetsMessage(
                        title: "Folder Access Removed",
                        text: "ARMSX2 will no longer use the saved permission for that folder."
                    )
                } label: {
                    Label(settings.localized("Remove Folder Access"), systemImage: "folder.badge.minus")
                }
            }
        } header: {
            Text(settings.localized("ARMSX2 Import Folder"))
        } footer: {
            Text(settings.localized("Selecting the ARMSX2 folder checks its BIOS, GAMES, PRESETS, and SKINS folders once. ZIP skins are imported, newly imported games receive missing covers, and a skin ZIP whose name starts with 1 becomes the default skin and layout. The saved permission is not scanned again on app launch; use Scan Selected Folder when you want to check it again. Existing imported files are not overwritten."))
                .lineLimit(nil)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    private var devicePresetsSection: some View {
        Section {
            ForEach(BuiltInSettingsPreset.allCases) { preset in
                Button {
                    preset.apply(settings: settings, skinLibrary: skinLibrary)
                } label: {
                    HStack(spacing: 12) {
                        VStack(alignment: .leading, spacing: 5) {
                            Text(settings.localized(preset.rawValue))
                                .font(.body.weight(.semibold))
                                .foregroundStyle(.primary)
                            Text(settings.localized(preset.summary))
                                .font(.caption)
                                .foregroundStyle(.secondary)
                                .multilineTextAlignment(.leading)
                                .lineLimit(nil)
                                .fixedSize(horizontal: false, vertical: true)
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .layoutPriority(1)

                        Spacer()

                        if preset.isActive(settings: settings, skinLibrary: skinLibrary) {
                            Image(systemName: "checkmark.circle.fill")
                                .foregroundStyle(.green)
                                .fixedSize()
                        } else {
                            Image(systemName: "chevron.forward")
                                .font(.caption.weight(.semibold))
                                .foregroundStyle(.tertiary)
                                .fixedSize()
                        }
                    }
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
        } header: {
            Text(settings.localized("Device Presets"))
        } footer: {
            Text(settings.localized(BuiltInSettingsPreset.allCases.map(\.detail).joined(separator: "\n\n") + " Selecting a preset changes only these listed settings."))
                .lineLimit(nil)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    private var presetFilesSection: some View {
        Section {
            Button {
                exportDocument = SettingsPresetDocument(
                    data: SettingsPresetFile.exportData(name: "ARMSX2 Custom Preset", settings: settings)
                )
                isExportingPreset = true
            } label: {
                Label(settings.localized("Export Current Preset"), systemImage: "square.and.arrow.up")
            }

            Button {
                presentedSheet = .presetImporter
            } label: {
                Label(settings.localized("Import Preset"), systemImage: "square.and.arrow.down")
            }
        } header: {
            Text(settings.localized("Preset Files"))
        } footer: {
            Text(settings.localized("Preset files use the .ini extension. Export saves the settings managed by this screen. Import validates and applies only recognized values; unknown keys are ignored."))
        }
    }

    private var presetImportContentTypes: [UTType] {
        var types: [UTType] = [.armsx2SettingsPreset, .plainText, .text, .data]
        if let iniType = UTType(filenameExtension: "ini") {
            types.insert(iniType, at: 0)
        }
        return Array(Set(types))
    }

    private func handleFolderPickerResult(_ result: Result<URL, Error>) {
        presentedSheet = nil
        switch result {
        case .success(let url):
            Task { @MainActor in
                let resultMessage = await folderAccess.selectARMSX2Folder(url)
                message = SettingsPresetsMessage(
                    title: "ARMSX2 Folder",
                    text: resultMessage
                )
            }
        case .failure(let error):
            if !FileImportHandler.isUserCancelledPickerError(error) {
                message = SettingsPresetsMessage(
                    title: "Folder Selection Failed",
                    text: error.localizedDescription
                )
            }
        }
    }

    private func scanSelectedFolder() {
        Task { @MainActor in
            let resultMessage = await folderAccess.scanSelectedFolder()
            message = SettingsPresetsMessage(
                title: "ARMSX2 Folder",
                text: resultMessage
            )
        }
    }

    private func handlePresetPickerResult(_ result: Result<[URL], Error>) {
        presentedSheet = nil
        switch result {
        case .success(let urls):
            guard let url = urls.first else { return }
            importPreset(at: url)
        case .failure(let error):
            if !FileImportHandler.isUserCancelledPickerError(error) {
                message = SettingsPresetsMessage(
                    title: "Preset Import Failed",
                    text: error.localizedDescription
                )
            }
        }
    }

    private func importPreset(at url: URL) {
        guard url.pathExtension.caseInsensitiveCompare("ini") == .orderedSame else {
            message = SettingsPresetsMessage(
                title: "Preset Import Failed",
                text: "Select an ARMSX2 preset with the .ini extension."
            )
            return
        }

        let stem = url.deletingPathExtension().lastPathComponent
        let accessing = url.startAccessingSecurityScopedResource()
        defer {
            if accessing {
                url.stopAccessingSecurityScopedResource()
            }
        }

        do {
            let selectedSkinID = skinLibrary.selectedSkinID
            let selectedVirtualPadSkin = settings.virtualPadSkin
            let data = try Data(contentsOf: url)
            let outcome = try SettingsPresetFile.importData(
                data,
                fallbackName: stem,
                settings: settings,
                skinLibrary: skinLibrary
            )
            var builtInNames = applyBuiltInPresets(named: stem)
            if outcome.name.caseInsensitiveCompare(stem) != .orderedSame {
                for name in applyBuiltInPresets(named: outcome.name)
                where !builtInNames.contains(name) {
                    builtInNames.append(name)
                }
            }
            let preservesVirtualPadSkin = builtInNames.contains { name in
                BuiltInSettingsPreset(rawValue: name)?.preservesVirtualPadSkin == true
            }
            if preservesVirtualPadSkin {
                skinLibrary.selectSkin(id: selectedSkinID)
                settings.virtualPadSkin = selectedVirtualPadSkin
            }
            let builtInSuffix = builtInNames.isEmpty
                ? ""
                : " Applied built-in preset: \(builtInNames.joined(separator: ", "))."
            message = SettingsPresetsMessage(
                title: "Preset Imported",
                text: "Applied \(outcome.appliedFieldCount) settings from \(outcome.name).\(builtInSuffix)"
            )
        } catch SettingsPresetFileError.noSupportedSettings {
            let builtInNames = applyBuiltInPresets(named: stem)
            if builtInNames.isEmpty {
                message = SettingsPresetsMessage(
                    title: "Preset Import Failed",
                    text: SettingsPresetFileError.noSupportedSettings.localizedDescription
                )
            } else {
                message = SettingsPresetsMessage(
                    title: "Preset Imported",
                    text: "Applied built-in preset: \(builtInNames.joined(separator: ", "))."
                )
            }
        } catch {
            message = SettingsPresetsMessage(
                title: "Preset Import Failed",
                text: error.localizedDescription
            )
        }
    }

    private func applyBuiltInPresets(named name: String) -> [String] {
        let normalizedName = name
            .trimmingCharacters(in: .whitespacesAndNewlines)
            .lowercased()
        var applied: [String] = []

        for preset in BuiltInSettingsPreset.allCases
        where preset.rawValue.lowercased() == normalizedName {
            preset.apply(settings: settings, skinLibrary: skinLibrary)
            applied.append(preset.rawValue)
        }
        for preset in BuiltInDynamicControlPreset.allCases
        where preset.rawValue.lowercased() == normalizedName {
            preset.apply()
            applied.append(preset.rawValue)
        }
        return applied
    }
}
