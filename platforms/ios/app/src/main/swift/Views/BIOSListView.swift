// BIOSListView.swift — BIOS file list with default selection
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UniformTypeIdentifiers

struct BIOSListView: View {
    @State private var bioses: [ARMSX2BIOSInfo] = []
    @State private var defaultBIOS: String = ""
    @State private var settings = SettingsStore.shared
    @State private var fileImporter = FileImportHandler.shared
    @State private var showBIOSImporter = false
    @State private var showBIOSCompatibilityImporter = false
    @State private var showBIOSReplacementAlert = false
    @State private var pendingBIOSImportURLs: [URL] = []
    @State private var existingBIOSImportFileNames: [String] = []
    @Environment(\.menuTabIsActive) private var menuTabIsActive

    private var backgroundActive: Bool {
        settings.hasCustomBackground && settings.backgroundEnabledInBIOS && menuTabIsActive
    }

    var body: some View {
        NavigationStack {
            ZStack {
                if backgroundActive {
                    MenuBackgroundLayer()
                }
                Group {
                    if bioses.isEmpty {
                        emptyState
                    } else {
                        List {
                            ForEach(bioses, id: \.self) { bios in
                                biosRow(bios)
                                    .menuBackgroundListRow(backgroundActive)
                            }
                        }
                        .scrollContentBackground(backgroundActive ? .hidden : .automatic)
#if targetEnvironment(macCatalyst)
                        .listStyle(.inset)
#endif
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }
            .navigationTitle(settings.localized("BIOS"))
            .toolbarBackground(backgroundActive ? .hidden : .automatic, for: .navigationBar)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    Menu {
                        Button {
                            presentMenuPanel("bios_import") {
                                NSLog("[ARMSX2 iOS BIOS] opening primary BIOS picker")
                                showBIOSImporter = true
                            }
                        } label: {
                            Label(settings.localized("Import BIOS"), systemImage: "doc.badge.plus")
                        }
                        Button {
                            presentMenuPanel("bios_compatibility_import") {
                                NSLog("[ARMSX2 iOS BIOS] opening compatibility BIOS picker")
                                showBIOSCompatibilityImporter = true
                            }
                        } label: {
                            Label(settings.localized("Compatibility Picker"), systemImage: "folder.badge.plus")
                        }
                    } label: {
                        Image(systemName: "plus")
                    }
                    .accessibilityLabel(settings.localized("Import BIOS"))
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button { loadBIOSes() } label: {
                        Image(systemName: "arrow.clockwise")
                    }
                }
            }
            .sheet(isPresented: $showBIOSImporter) {
                ImportDocumentPicker(
                    allowedContentTypes: FileImportHandler.biosContentTypes,
                    allowsMultipleSelection: true
                ) { result in
                    showBIOSImporter = false
                    handleBIOSPickerResult(result, source: "primary")
                }
            }
            .sheet(isPresented: $showBIOSCompatibilityImporter) {
                ImportDocumentPicker(
                    allowedContentTypes: FileImportHandler.biosContentTypes,
                    allowsMultipleSelection: true,
                    legacyDocumentTypes: ["public.item", "public.data", "public.content"]
                ) { result in
                    showBIOSCompatibilityImporter = false
                    handleBIOSPickerResult(result, source: "compatibility")
                }
            }
            .alert(settings.localized("Replace existing files?"), isPresented: $showBIOSReplacementAlert) {
                Button(settings.localized("Cancel"), role: .cancel) {
                    clearPendingBIOSImport()
                }
                Button(settings.localized("Replace"), role: .destructive) {
                    importBIOSFiles(pendingBIOSImportURLs, allowReplacingExistingFiles: true)
                    clearPendingBIOSImport()
                }
            } message: {
                Text(FileImportHandler.replacementConfirmationMessage(for: existingBIOSImportFileNames))
            }
        }
        .onAppear { loadBIOSes() }
    }

    private func presentMenuPanel(_ name: String, _ action: @escaping () -> Void) {
        NSLog("[ARMSX2 iOS BIOSMenu] present \(name)")
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.12) {
            action()
        }
    }

    private func biosRow(_ bios: ARMSX2BIOSInfo) -> some View {
        Button {
            if bios.valid {
                ARMSX2Bridge.setDefaultBIOS(bios.fileName)
                defaultBIOS = bios.fileName
            } else {
                fileImporter.lastImportMessage = "\(bios.fileName) is visible in your BIOS folder, but it is not a bootable PS2 BIOS. Keep it if it is a companion ROM, and select a valid boot BIOS as default."
                fileImporter.showImportAlert = true
            }
        } label: {
            HStack(spacing: 12) {
                regionBadge(for: bios)

                VStack(alignment: .leading, spacing: 4) {
                    Text(bios.fileName)
                        .font(.body)
                        .foregroundStyle(.primary)
                    Text(bios.valid ? "\(bios.regionName) BIOS" : settings.localized("Not a boot BIOS"))
                        .font(.caption)
                        .foregroundStyle(.secondary)
                    if bios.valid && !bios.descriptionText.isEmpty {
                        Text(bios.descriptionText)
                            .font(.caption2)
                            .foregroundStyle(.tertiary)
                            .lineLimit(1)
                    } else if !bios.valid {
                        Text(settings.localized("Companion ROM or unsupported BIOS dump"))
                            .font(.caption2)
                            .foregroundStyle(.tertiary)
                            .lineLimit(1)
                    }
                }
                Spacer()
                if bios.fileName == defaultBIOS {
                    Image(systemName: "checkmark.circle.fill")
                        .foregroundStyle(.blue)
                }
            }
        }
        .foregroundStyle(.primary)
        .opacity(bios.valid ? 1 : 0.65)
    }

    private var emptyState: some View {
        VStack(spacing: 16) {
            Image(systemName: "cpu")
                .font(.system(size: 48))
                .foregroundStyle(.secondary)
            Text(settings.localized("No BIOS Found"))
                .font(.title2)
                .fontWeight(.semibold)
            Text(settings.localized("Import a PS2 BIOS dump to enable booting."))
                .font(.body)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
            Button {
                NSLog("[ARMSX2 iOS BIOS] opening primary BIOS picker from empty state")
                showBIOSImporter = true
            } label: {
                Label(settings.localized("Import BIOS"), systemImage: "plus")
            }
            .buttonStyle(.borderedProminent)
            Text(settings.localized("If one picker refuses to select your .bin/.rom file, try the other."))
                .font(.caption)
                .foregroundStyle(.tertiary)
                .multilineTextAlignment(.center)
        }
        .padding()
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    private func handleBIOSPickerResult(_ result: Result<[URL], Error>, source: String) {
        switch result {
        case .success(let urls):
            NSLog("[ARMSX2 iOS BIOS] %@ picker completed with %d URL(s)", source, urls.count)
            prepareBIOSImport(urls)
        case .failure(let error):
            if !FileImportHandler.isUserCancelledPickerError(error) {
                fileImporter.presentImportResult(FileImportHandler.failedBIOSPickerMessage(errorDescription: error.localizedDescription))
            }
        }
    }

    private func prepareBIOSImport(_ urls: [URL]) {
        let existingFileNames = fileImporter.existingFileNames(for: urls, preferredDestination: .bios)
        guard !existingFileNames.isEmpty else {
            importBIOSFiles(urls, allowReplacingExistingFiles: false)
            return
        }

        pendingBIOSImportURLs = urls
        existingBIOSImportFileNames = existingFileNames
        showBIOSReplacementAlert = true
    }

    private func importBIOSFiles(_ urls: [URL], allowReplacingExistingFiles: Bool) {
        fileImporter.handleURLs(
            urls,
            preferredDestination: .bios,
            allowReplacingExistingFiles: allowReplacingExistingFiles
        )
        loadBIOSes()
        if defaultBIOS.isEmpty, let firstBIOS = bioses.first(where: { $0.valid })?.fileName {
            ARMSX2Bridge.setDefaultBIOS(firstBIOS)
            defaultBIOS = firstBIOS
        }
        if let guidance = nonBootableImportGuidance(for: urls) {
            let message = [
                fileImporter.lastImportMessage,
                guidance
            ]
            .compactMap { $0 }
            .joined(separator: "\n")
            fileImporter.presentImportResult(message)
        } else if !bioses.contains(where: { $0.valid }), !urls.isEmpty {
            let message = [
                fileImporter.lastImportMessage,
                "No bootable PS2 BIOS was found. Import a valid PS2 BIOS dump before starting games."
            ]
            .compactMap { $0 }
            .joined(separator: "\n")
            fileImporter.presentImportResult(message)
        }
    }

    private func clearPendingBIOSImport() {
        pendingBIOSImportURLs = []
        existingBIOSImportFileNames = []
    }

    private func loadBIOSes() {
        bioses = ARMSX2Bridge.availableBIOSInfos()
        defaultBIOS = ARMSX2Bridge.defaultBIOSName()
    }

    private func nonBootableImportGuidance(for urls: [URL]) -> String? {
        let selectedFileNames = Set(urls.map(\.lastPathComponent))
        let nonBootableFileNames = bioses
            .filter { !$0.valid && selectedFileNames.contains($0.fileName) }
            .map(\.fileName)

        guard !nonBootableFileNames.isEmpty else { return nil }

        let fileMessage: String
        let setupMessage: String
        if nonBootableFileNames.count == 1 {
            fileMessage = "\(nonBootableFileNames[0]) is in your BIOS folder, but it is not a bootable PS2 BIOS. It may be a companion ROM or unsupported BIOS-related file."
            setupMessage = "A bootable BIOS is already installed, but this selected file cannot be used to boot games."
        } else {
            fileMessage = "These selected files are in your BIOS folder, but they are not bootable PS2 BIOS files: \(nonBootableFileNames.joined(separator: ", ")). They may be companion ROMs or unsupported BIOS-related files."
            setupMessage = "A bootable BIOS is already installed, but these files cannot be used to boot games."
        }

        if bioses.contains(where: { $0.valid }) {
            return "\(fileMessage)\n\(setupMessage)"
        }
        return "\(fileMessage)\nNo bootable PS2 BIOS was found. Import a valid PS2 BIOS dump before starting games."
    }

    @ViewBuilder
    private func regionBadge(for bios: ARMSX2BIOSInfo) -> some View {
        if backgroundActive {
            badgeContent(for: bios)
                .frame(width: 44, height: 44)
                .glassSurface(cornerRadius: 12)
                .accessibilityLabel(bios.valid ? "\(bios.regionName) BIOS" : settings.localized("Not a boot BIOS"))
        } else {
            badgeContent(for: bios)
                .frame(width: 44, height: 44)
                .background(
                    Color(.secondarySystemGroupedBackground),
                    in: RoundedRectangle(cornerRadius: 12, style: .continuous)
                )
                .accessibilityLabel(bios.valid ? "\(bios.regionName) BIOS" : settings.localized("Not a boot BIOS"))
        }
    }

    @ViewBuilder
    private func badgeContent(for bios: ARMSX2BIOSInfo) -> some View {
        if let flag = flagEmoji(for: bios.countryCode) {
            Text(flag)
                .font(.title2)
        } else {
            Image(systemName: "globe")
                .font(.title3)
                .foregroundStyle(.secondary)
        }
    }

    private func flagEmoji(for countryCode: String) -> String? {
        let scalars = countryCode.uppercased().unicodeScalars
        guard scalars.count == 2 else { return nil }

        var unicodeScalars = String.UnicodeScalarView()
        for scalar in scalars {
            guard scalar.value >= 65, scalar.value <= 90,
                  let regional = UnicodeScalar(0x1F1E6 + scalar.value - 65) else {
                return nil
            }
            unicodeScalars.append(regional)
        }

        return String(unicodeScalars)
    }
}
