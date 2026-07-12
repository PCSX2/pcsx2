// BackgroundSourcePicker.swift — Photos/Files picker modifier for backgrounds
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import PhotosUI
import UniformTypeIdentifiers

enum BackgroundAssetRole: String, CaseIterable, Identifiable {
    case primary, landscape
    var id: String { rawValue }
}

struct BackgroundSourcePicker: ViewModifier {
    @Binding var isPresented: Bool
    let role: BackgroundAssetRole
    let existingAsset: () -> BackgroundAsset?
    let onImport: (BackgroundAsset?) -> Void

    @State private var showPhotoPicker = false
    @State private var showFilePicker = false
    @State private var showLargeFileWarning = false
    @State private var showLoadError = false
    @State private var selectedPhotoItem: PhotosPickerItem?
    @State private var pendingFileURL: URL?

    private var settings: SettingsStore { SettingsStore.shared }
    private var currentAsset: BackgroundAsset? { existingAsset() }

    func body(content: Content) -> some View {
        content
            .confirmationDialog(dialogTitle, isPresented: $isPresented, titleVisibility: .visible) {
                Button(settings.localized("Choose from Photos")) { showPhotoPicker = true }
                Button(settings.localized("Choose from Files")) { showFilePicker = true }
                if currentAsset != nil { Button(settings.localized("Remove"), role: .destructive) { onImport(nil) } }
                Button(settings.localized("Cancel"), role: .cancel) {}
            }
            .photosPicker(isPresented: $showPhotoPicker, selection: $selectedPhotoItem, matching: .any(of: [.images, .videos]))
            .onChange(of: selectedPhotoItem) { _, item in
                guard let item else { return }
                selectedPhotoItem = nil
                Task { @MainActor in await importPhotoItem(item) }
            }
            .fileImporter(isPresented: $showFilePicker, allowedContentTypes: [.image, .audiovisualContent], allowsMultipleSelection: false) { handleFileImport($0) }
            .alert(settings.localized("Large Background File"), isPresented: $showLargeFileWarning) {
                Button(settings.localized("Cancel"), role: .cancel) { pendingFileURL = nil }
                Button(settings.localized("Import Anyway")) { importPendingFileURL() }
            } message: {
                Text(settings.localized("This background file is very large and may affect performance or battery life."))
            }
            .alert(settings.localized("Background image could not be loaded."), isPresented: $showLoadError) {
                Button(settings.localized("OK")) {}
            }
    }

    private var dialogTitle: String {
        switch role {
        case .primary: return settings.localized("Library Background")
        case .landscape: return settings.localized("Landscape Background")
        }
    }

    private func importPhotoItem(_ item: PhotosPickerItem) async {
        do {
            guard let data = try await item.loadTransferable(type: Data.self) else {
                showLoadError = true
                return
            }
            let ext = await item.supportedContentTypes.first?.preferredFilenameExtension ?? "jpg"
            let replacing = currentAsset
            let asset = try await Task.detached(priority: .userInitiated) {
                try BackgroundStorage.importData(data, fileExtension: ext, replacing: replacing)
            }.value
            onImport(asset)
        } catch {
            showLoadError = true
            NSLog("[ARMSX2 Background] photo import failed: %@", error.localizedDescription)
        }
    }

    private func handleFileImport(_ result: Result<[URL], Error>) {
        switch result {
        case .success(let urls):
            guard let url = urls.first else { return }
            if BackgroundStorage.isLarge(url) { pendingFileURL = url; showLargeFileWarning = true } else { importFile(url) }
        case .failure(let error):
            if !isUserCancelled(error) { showLoadError = true }
        }
    }

    private func importPendingFileURL() {
        guard let url = pendingFileURL else { return }
        importFile(url)
        pendingFileURL = nil
    }

    private func importFile(_ url: URL) {
        Task { @MainActor in
            do {
                let replacing = currentAsset
                let asset = try await Task.detached(priority: .userInitiated) {
                    let accessing = url.startAccessingSecurityScopedResource()
                    defer { if accessing { url.stopAccessingSecurityScopedResource() } }
                    return try BackgroundStorage.importFile(from: url, replacing: replacing)
                }.value
                onImport(asset)
            } catch {
                showLoadError = true
                NSLog("[ARMSX2 Background] file import failed: %@", error.localizedDescription)
            }
        }
    }

    private func isUserCancelled(_ error: Error) -> Bool {
        let ns = error as NSError
        return ns.domain == "NSCocoaErrorDomain" && ns.code == 3072
    }
}
