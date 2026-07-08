// VirtualPadSettingsView.swift — Virtual pad opacity, haptic, layout editing
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit
import UniformTypeIdentifiers

struct VirtualPadSettingsView: View {
    @State private var settings = SettingsStore.shared
    @State private var layoutPresets = PadLayoutPresetStore.shared
    @State private var skinLibrary = VPadSkinLibraryStore.shared
    @State private var showLayoutEditor = false
    @State private var showSkinImporter = false
    @State private var showSkinImportAlert = false
    @State private var lastSkinImportResult: VPadSkinImportResult?
    @State private var skinImportMessage = ""
    @State private var showLayoutImporter = false
    @State private var showLayoutImportAlert = false
    @State private var layoutImportMessage = ""
    @State private var layoutExportItem: ShareSheetItem?
    @State private var skinPendingDelete: VPadSkinDescriptor?
    @State private var skinPendingRename: VPadSkinDescriptor?
    @State private var skinRenameDraft = ""

    var body: some View {
        Form {
            Section(settings.localized("Appearance")) {
                Picker(settings.localized("Button Skin"), selection: Binding<String>(
                    get: { skinLibrary.selectedSkinID },
                    set: { selectSkin(id: $0) }
                )) {
                    ForEach(skinLibrary.allDescriptors) { skin in
                        Text(settings.localized(skin.displayName)).tag(skin.id)
                    }
                }

                Text(settings.localized(selectedSkinDetail))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                VStack(alignment: .leading) {
                    Text("\(settings.localized("Opacity")): \(Int(settings.padOpacity * 100))%")
                    Slider(value: $settings.padOpacity, in: 0.1...1.0, step: 0.05)
                }
            }

            Section(settings.localized("Gameplay")) {
                Toggle(settings.localized("Hide Virtual Pad When Controller Is Connected"), isOn: $settings.autoHideVirtualPadWhenControllerConnected)
                Text(settings.localized("Automatically hides the on-screen controls while an external controller is connected."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Toggle(settings.localized("Auto Full Screen"), isOn: $settings.autoFullscreen)
                Toggle(settings.localized("Hide Menu Button"), isOn: $settings.hideMenuButton)

                Toggle(settings.localized("D-pad Diagonals"), isOn: $settings.dpadDiagonalsEnabled)
                Text(settings.localized("Allows one-finger diagonal and quarter-circle motions on the virtual D-pad."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Toggle(settings.localized("Face Button Combo Zones"), isOn: $settings.faceComboZonesEnabled)
                Text(settings.localized("Press between face buttons to trigger both buttons at once."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        Text(settings.localized("Analog Stick Size"))
                        Spacer()
                        Text("\(Int((settings.analogStickScale * 100).rounded()))%")
                            .foregroundStyle(.secondary)
                    }
                    Slider(
                        value: Binding(
                            get: { Double(settings.analogStickScale) },
                            set: { settings.analogStickScale = Float($0) }
                        ),
                        in: 0.8...1.6,
                        step: 0.05
                    )
                }

                Text(settings.localized("Double-tap empty gameplay space to show the menu button again."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            Section(settings.localized("Custom Skin")) {
                Button {
                    showSkinImporter = true
                } label: {
                    Label(settings.localized("Import Skin"), systemImage: "paintpalette")
                }

                Text("Import loose PNG/JPG/WebP button images, a full portrait/landscape controller image, or a zipped skin pack. Button files can be named cross, circle, square, triangle, up, down, left, right, L1, R1, L2, R2, start, select, analog_base, or analog_stick.")
                    .font(.caption)
                    .foregroundStyle(.secondary)

                if !skinLibrary.importedDescriptors.isEmpty {
                    ForEach(skinLibrary.importedDescriptors) { skin in
                        HStack {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(skin.displayName)
                                if skin.linkedLayoutPresetID != nil {
                                    Text("Includes recommended layout")
                                        .font(.caption)
                                        .foregroundStyle(.secondary)
                                }
                            }
                            Spacer()
                            Menu {
                                Button {
                                    selectSkin(id: skin.id)
                                } label: {
                                    Label("Set as Global Default", systemImage: "checkmark.circle")
                                }
                                Button {
                                    skinPendingRename = skin
                                    skinRenameDraft = skin.displayName
                                } label: {
                                    Label("Rename Skin", systemImage: "pencil")
                                }
                                Button(role: .destructive) {
                                    skinPendingDelete = skin
                                } label: {
                                    Label("Delete Skin", systemImage: "trash")
                                }
                            } label: {
                                Image(systemName: "ellipsis.circle")
                            }
                        }
                    }
                }
            }

            Section(settings.localized("Feedback")) {
                Toggle(settings.localized("Haptic Feedback"), isOn: $settings.hapticFeedback)
            }

            Section(settings.localized("Layout")) {
                Picker("Default VPad Layout", selection: Binding<String?>(
                    get: { layoutPresets.globalPresetID },
                    set: { layoutPresets.globalPresetID = $0 }
                )) {
                    Text("Current Layout").tag(nil as String?)
                    ForEach(layoutPresets.presets) { preset in
                        Text(preset.displayName).tag(Optional(preset.id))
                    }
                }

                Button {
                    showLayoutEditor = true
                } label: {
                    Label(settings.localized("Edit Layout"), systemImage: "square.resize")
                }
                Text(settings.localized("Drag buttons to reposition. Pinch to resize."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Text("Simple custom pad skins are shown behind the blue hit boxes in Edit Layout. Advanced skin packages can include their own PS2 control layout metadata. Non-PS2 Delta/Manic skins are not converted automatically.")
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Button {
                    showLayoutImporter = true
                } label: {
                    Label("Import Layout", systemImage: "square.and.arrow.down")
                }

                if !layoutPresets.presets.isEmpty {
                    ForEach(layoutPresets.presets) { preset in
                        HStack {
                            Text(preset.displayName)
                                .lineLimit(1)
                            Spacer()
                            Button {
                                exportLayout(preset)
                            } label: {
                                Image(systemName: "square.and.arrow.up")
                            }
                            .buttonStyle(.borderless)
                            .accessibilityLabel("Export \(preset.displayName)")
                        }
                    }
                }
            }
        }
        .navigationTitle(settings.localized("Virtual Pad"))
        .navigationBarTitleDisplayMode(.inline)
        .sheet(isPresented: $showLayoutImporter) {
            ImportDocumentPicker(
                allowedContentTypes: [.json, .data],
                allowsMultipleSelection: true
            ) { result in
                switch result {
                case .success(let urls):
                    layoutImportMessage = importLayouts(urls)
                case .failure(let error):
                    layoutImportMessage = "Layout import failed: \(error.localizedDescription)"
                }
                showLayoutImportAlert = true
            }
        }
        .sheet(item: $layoutExportItem) { item in
            ActivityShareSheet(activityItems: [item.url])
        }
        .alert("Layout Import", isPresented: $showLayoutImportAlert) {
            Button(settings.localized("OK"), role: .cancel) {}
        } message: {
            Text(layoutImportMessage)
        }
        .sheet(isPresented: $showSkinImporter) {
            ImportDocumentPicker(
                allowedContentTypes: [
                    .image,
                    UTType(filenameExtension: "zip") ?? .data,
                    UTType(filenameExtension: "skin") ?? .data,
                    UTType(filenameExtension: "manic") ?? .data,
                    UTType(filenameExtension: "armsx2skin") ?? .data,
                    UTType(filenameExtension: "deltaskin") ?? .data,
                    UTType(filenameExtension: "manicskin") ?? .data,
                    .data
                ],
                allowsMultipleSelection: true
            ) { result in
                switch result {
                case .success(let urls):
                    let result = importCustomSkins(urls)
                    lastSkinImportResult = result.importResult
                    skinImportMessage = result.message
                case .failure(let error):
                    lastSkinImportResult = nil
                    skinImportMessage = "Skin import failed: \(error.localizedDescription)"
                }
                showSkinImportAlert = true
            }
        }
        .alert(settings.localized("Custom Skin"), isPresented: $showSkinImportAlert) {
            if let result = lastSkinImportResult {
                if result.includesLinkedLayout {
                    Button("Apply Skin Only Globally") {
                        selectSkin(id: result.descriptor.id)
                    }
                    Button("Apply Skin + Layout Globally") {
                        selectSkin(id: result.descriptor.id)
                        layoutPresets.globalPresetID = result.descriptor.linkedLayoutPresetID
                    }
                    Button("Apply Layout Only Globally") {
                        layoutPresets.globalPresetID = result.descriptor.linkedLayoutPresetID
                    }
                    Button("Later", role: .cancel) {}
                } else {
                    Button("Apply Skin Only Globally") {
                        selectSkin(id: result.descriptor.id)
                    }
                    Button("Later", role: .cancel) {}
                }
            } else {
                Button(settings.localized("OK"), role: .cancel) {}
            }
        } message: {
            Text(skinImportMessage)
        }
        .alert("Rename Skin", isPresented: Binding<Bool>(
            get: { skinPendingRename != nil },
            set: { if !$0 { skinPendingRename = nil } }
        )) {
            TextField("Name", text: $skinRenameDraft)
            Button("Save") {
                if let skin = skinPendingRename {
                    try? skinLibrary.renameImportedSkin(id: skin.id, to: skinRenameDraft)
                }
                skinPendingRename = nil
            }
            Button("Cancel", role: .cancel) {
                skinPendingRename = nil
            }
        } message: {
            Text("Choose a display name for this imported skin.")
        }
        .confirmationDialog(
            "Delete Skin?",
            isPresented: Binding<Bool>(
                get: { skinPendingDelete != nil },
                set: { if !$0 { skinPendingDelete = nil } }
            ),
            presenting: skinPendingDelete
        ) { skin in
            Button("Delete \(skin.displayName)", role: .destructive) {
                try? skinLibrary.deleteImportedSkin(id: skin.id, layoutPresets: layoutPresets)
                syncSettingsSkinFromLibrarySelection()
                skinPendingDelete = nil
            }
            Button("Cancel", role: .cancel) {
                skinPendingDelete = nil
            }
        } message: { skin in
            Text("This removes the imported skin. Linked layout presets are kept.")
        }
        .fullScreenCover(isPresented: $showLayoutEditor) {
            PadLayoutEditView(
                onDismiss: { showLayoutEditor = false },
                context: PadLayoutEditorContext(
                    presetID: layoutPresets.globalPresetID,
                    gameIdentity: nil,
                    initialSnapshot: layoutPresets.effectiveSnapshot(for: nil)
                )
            )
        }
    }

    private var selectedSkinDetail: String {
        let descriptor = skinLibrary.selectedDescriptor
        if descriptor.source == .imported {
            if descriptor.linkedLayoutPresetID != nil {
                return "Uses imported controller art. A recommended layout is saved separately and only applies when selected."
            }
            return "Uses imported controller art without changing the active layout."
        }
        return descriptor.virtualPadSkin.detail
    }

    private func selectSkin(id: String) {
        skinLibrary.selectSkin(id: id)
        syncSettingsSkinFromLibrarySelection()
    }

    private func syncSettingsSkinFromLibrarySelection() {
        settings.virtualPadSkin = skinLibrary.selectedDescriptor.virtualPadSkin
    }

    private func importLayouts(_ urls: [URL]) -> String {
        var messages: [String] = []
        for sourceURL in urls {
            let accessGranted = sourceURL.startAccessingSecurityScopedResource()
            defer {
                if accessGranted {
                    sourceURL.stopAccessingSecurityScopedResource()
                }
            }
            do {
                let data = try Data(contentsOf: sourceURL)
                let preset = try layoutPresets.importLayout(data: data, fallbackName: sourceURL.lastPathComponent)
                messages.append("Imported layout '\(preset.displayName)'.")
            } catch {
                messages.append("Layout import failed for \(sourceURL.lastPathComponent): \(error.localizedDescription)")
            }
        }
        return messages.isEmpty ? "No layout files were selected." : messages.joined(separator: "\n\n")
    }

    private func exportLayout(_ preset: PadLayoutPreset) {
        do {
            let data = try PadLayoutImportExport.exportData(for: preset)
            let url = FileManager.default.temporaryDirectory
                .appendingPathComponent(PadLayoutImportExport.exportedFileName(for: preset.displayName))
            try data.write(to: url, options: .atomic)
            layoutExportItem = ShareSheetItem(url: url)
        } catch {
            layoutImportMessage = "Layout export failed: \(error.localizedDescription)"
            showLayoutImportAlert = true
        }
    }

    private func importCustomSkins(_ urls: [URL]) -> (message: String, importResult: VPadSkinImportResult?) {
        let stagingDirectory = FileManager.default.temporaryDirectory
            .appendingPathComponent("ARMSX2SkinImport-\(UUID().uuidString)", isDirectory: true)
        try? FileManager.default.createDirectory(at: stagingDirectory, withIntermediateDirectories: true)
        defer {
            try? FileManager.default.removeItem(at: stagingDirectory)
        }

        var messages: [String] = []
        var latestResult: VPadSkinImportResult?
        let looseFiles = urls.filter { !isSkinArchive($0) }
        let archiveFiles = urls.filter { isSkinArchive($0) }

        if !looseFiles.isEmpty {
            let looseDirectory = stagingDirectory.appendingPathComponent("LooseSkin", isDirectory: true)
            try? FileManager.default.createDirectory(at: looseDirectory, withIntermediateDirectories: true)
            for sourceURL in looseFiles {
                let accessGranted = sourceURL.startAccessingSecurityScopedResource()
                defer {
                    if accessGranted {
                        sourceURL.stopAccessingSecurityScopedResource()
                    }
                }
                let destination = looseDirectory.appendingPathComponent(sourceURL.lastPathComponent)
                try? FileManager.default.removeItem(at: destination)
                try? FileManager.default.copyItem(at: sourceURL, to: destination)
            }
            do {
                let result = try skinLibrary.importSkin(
                    from: looseDirectory,
                    originalImportName: looseFiles.first?.lastPathComponent,
                    layoutPresets: layoutPresets
                )
                latestResult = result
                messages.append(result.message)
            } catch {
                messages.append("Skin import failed: \(error.localizedDescription)")
            }
        }

        for sourceURL in archiveFiles {
            let accessGranted = sourceURL.startAccessingSecurityScopedResource()
            defer {
                if accessGranted {
                    sourceURL.stopAccessingSecurityScopedResource()
                }
            }

            let archiveDirectory = stagingDirectory
                .appendingPathComponent(sourceURL.deletingPathExtension().lastPathComponent, isDirectory: true)
            let isV2Package = SkinManifestImporter.shouldTreatAsV2(
                manifestData: ARMSX2Bridge.peekSkinManifestData(at: sourceURL)
            )
            let extracted: [URL]
            if isV2Package {
                extracted = ARMSX2Bridge.extractSkinPackageArchive(at: sourceURL, to: archiveDirectory)
            } else {
                extracted = ARMSX2Bridge.extractControllerSkinArchive(at: sourceURL, to: archiveDirectory)
            }
            if extracted.isEmpty {
                messages.append("No usable skin files were imported from \(sourceURL.lastPathComponent).")
                continue
            }
            do {
                let result = try skinLibrary.importSkin(
                    from: archiveDirectory,
                    originalImportName: sourceURL.lastPathComponent,
                    layoutPresets: layoutPresets
                )
                latestResult = result
                messages.append(result.message)
            } catch {
                messages.append("Skin import failed: \(error.localizedDescription)")
            }
        }

        let message = messages.isEmpty
            ? "No usable skin images were imported. Use loose button PNGs/JPGs/WebPs, a portrait/landscape controller image, or a zip skin pack containing image files."
            : messages.joined(separator: "\n\n")
        return (message, latestResult)
    }

    private func isSkinArchive(_ url: URL) -> Bool {
        let ext = url.pathExtension.lowercased()
        return ext == "zip" || ext == "skin" || ext == "manic"
            || ext == "armsx2skin" || ext == "deltaskin" || ext == "manicskin"
    }

    static func canonicalSkinFileName(forImportPath path: String) -> String? {
        VPadSkinLibraryStore.canonicalSkinFileName(forImportPath: path)
    }
}
