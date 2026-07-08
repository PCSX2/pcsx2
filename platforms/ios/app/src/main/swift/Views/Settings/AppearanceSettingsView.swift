// AppearanceSettingsView.swift — Library background customization
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import PhotosUI
import UIKit
import ImageIO

struct AppearanceSettingsView: View {
    @State private var settings = SettingsStore.shared
    @State private var selectedPhotoItem: PhotosPickerItem?
    @State private var showPhotoPicker = false
    @State private var showLoadError = false
    @State private var selectedBackgroundRole = LibraryBackgroundRole.main

    private var hasBackground: Bool {
        !settings.libraryBackgroundPath.isEmpty && FileManager.default.fileExists(atPath: settings.libraryBackgroundPath)
    }

    private var hasLandscapeBackground: Bool {
        !settings.libraryLandscapeBackgroundPath.isEmpty && FileManager.default.fileExists(atPath: settings.libraryLandscapeBackgroundPath)
    }

    var body: some View {
        Form {
            Section(settings.localized("App Icon")) {
                NavigationLink {
                    AppIconSettingsView()
                } label: {
                    Label(settings.localized("App Icon"), systemImage: "app.badge")
                }
            }
            Section(settings.localized("Library Background")) {
                Text(settings.localized("Use a custom image behind your game library. Animated GIF and APNG are supported; large or still images fall back to a single frame."))
                    .font(.caption)
                    .foregroundStyle(.secondary)

                Button {
                    selectedBackgroundRole = .main
                    showPhotoPicker = true
                } label: {
                    Label(settings.localized("Choose Background"), systemImage: "photo")
                }

                if hasBackground {
                    Button(role: .destructive) {
                        removeLibraryBackground()
                    } label: {
                        Label(settings.localized("Remove Background"), systemImage: "trash")
                    }

                    VStack(alignment: .leading, spacing: 6) {
                        Text(settings.localized("Background Dim"))
                            .font(.subheadline)
                        Slider(
                            value: $settings.libraryBackgroundDim,
                            in: 0.0...0.8,
                            step: 0.05
                        )
                        .accessibilityLabel(settings.localized("Background Dim"))
                        .accessibilityValue(String(format: "%.0f%%", settings.libraryBackgroundDim * 100))
                    }
                    .padding(.vertical, 4)
                }
            }

            if hasBackground {
                Section(settings.localized("Landscape Background")) {
                    Text(settings.localized("Uses the main background when no landscape image is selected."))
                        .font(.caption)
                        .foregroundStyle(.secondary)

                    Button {
                        selectedBackgroundRole = .landscape
                        showPhotoPicker = true
                    } label: {
                        Label(settings.localized("Choose Landscape Background"), systemImage: "rectangle.landscape")
                    }

                    if hasLandscapeBackground {
                        Button(role: .destructive) {
                            removeLandscapeBackground()
                        } label: {
                            Label(settings.localized("Remove Landscape Background"), systemImage: "trash")
                        }
                    }
                }
            }
        }
        .navigationTitle(settings.localized("Appearance"))
        .photosPicker(
            isPresented: $showPhotoPicker,
            selection: $selectedPhotoItem,
            matching: .images
        )
        .onChange(of: selectedPhotoItem) { _, newItem in
            guard let newItem else { return }
            let role = selectedBackgroundRole
            selectedPhotoItem = nil
            Task { @MainActor in
                await importLibraryBackground(from: newItem, role: role)
            }
        }
        .alert(
            settings.localized("Background image could not be loaded."),
            isPresented: $showLoadError
        ) {
            Button(settings.localized("OK")) {}
        }
    }

    private func importLibraryBackground(from photoItem: PhotosPickerItem, role: LibraryBackgroundRole) async {
        do {
            guard let data = try await photoItem.loadTransferable(type: Data.self),
                  let image = UIImage(data: data) else {
                showLoadError = true
                return
            }
            // Preserve animated images (GIF/APNG/animated WebP) as their
            // original bytes so playback works; re-encoding to JPEG would
            // collapse them to a single frame. Static images still go through
            // the resized JPEG path.
            let savedPath: String
            if AnimatedBackgroundImport.shouldKeepAsAnimated(data) {
                savedPath = try LibraryBackgroundHelper.saveAnimated(data, for: role)
            } else {
                savedPath = try LibraryBackgroundHelper.save(image, for: role)
            }
            switch role {
            case .main:
                settings.libraryBackgroundPath = savedPath
            case .landscape:
                settings.libraryLandscapeBackgroundPath = savedPath
            }
            settings.libraryBackgroundRevision &+= 1
        } catch {
            showLoadError = true
            NSLog("[ARMSX2 iOS LibraryBackground] import failed: %@", error.localizedDescription)
        }
    }

    private func removeLibraryBackground() {
        LibraryBackgroundHelper.remove(.main)
        LibraryBackgroundHelper.remove(.landscape)
        settings.libraryBackgroundPath = ""
        settings.libraryLandscapeBackgroundPath = ""
        settings.libraryBackgroundDim = 0.35
        settings.libraryBackgroundRevision &+= 1
    }

    private func removeLandscapeBackground() {
        LibraryBackgroundHelper.remove(.landscape)
        settings.libraryLandscapeBackgroundPath = ""
        settings.libraryBackgroundRevision &+= 1
    }
}

private enum LibraryBackgroundRole {
    case main
    case landscape

    var fileName: String {
        switch self {
        case .main:
            return "library_background.jpg"
        case .landscape:
            return "library_background_landscape.jpg"
        }
    }
}

private enum LibraryBackgroundHelper {
    static let maxDimension: CGFloat = 1920
    static let jpegQuality: CGFloat = 0.85

    static var storageDirectory: URL {
        let fm = FileManager.default
        let urls = fm.urls(for: .applicationSupportDirectory, in: .userDomainMask)
        let dir = urls.first?.appendingPathComponent("ARMSX2", isDirectory: true) ?? fm.temporaryDirectory.appendingPathComponent("ARMSX2", isDirectory: true)
        try? fm.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir
    }

    static func fileURL(for role: LibraryBackgroundRole) -> URL {
        storageDirectory.appendingPathComponent(role.fileName)
    }

    static func save(_ image: UIImage, for role: LibraryBackgroundRole) throws -> String {
        let resized = resize(image, maxDimension: maxDimension)
        guard let data = resized.jpegData(compressionQuality: jpegQuality) else {
            throw NSError(domain: "LibraryBackground", code: 1, userInfo: [NSLocalizedDescriptionKey: "JPEG encoding failed"])
        }
        let url = fileURL(for: role)
        try data.write(to: url, options: .atomic)
        return url.path
    }

    static func remove(_ role: LibraryBackgroundRole) {
        let url = fileURL(for: role)
        if FileManager.default.fileExists(atPath: url.path) {
            try? FileManager.default.removeItem(at: url)
        }
    }

    private static func resize(_ image: UIImage, maxDimension: CGFloat) -> UIImage {
        let size = image.size
        let maxSide = max(size.width, size.height)
        guard maxSide > maxDimension else { return image }
        let scale = maxDimension / maxSide
        let newSize = CGSize(width: size.width * scale, height: size.height * scale)
        let renderer = UIGraphicsImageRenderer(size: newSize)
        return renderer.image { _ in
            image.draw(in: CGRect(origin: .zero, size: newSize))
        }
    }

    /// Writes the original animated bytes verbatim so playback stays intact.
    /// Caps the file size; anything larger should have gone through the static
    /// path (callers check `AnimatedBackgroundImport.shouldKeepAsAnimated`).
    static func saveAnimated(_ data: Data, for role: LibraryBackgroundRole) throws -> String {
        let ext = AnimatedBackgroundImport.fileExtension(for: data)
        let fileName: String
        switch role {
        case .main: fileName = "library_background.\(ext)"
        case .landscape: fileName = "library_background_landscape.\(ext)"
        }
        // Remove any prior background of any extension for this role so we do
        // not accumulate stale files (e.g. an old .jpg left beside a new .gif).
        for stale in ["jpg", "gif", "apng", "webp"] {
            let staleURL = storageDirectory.appendingPathComponent(
                role == .main ? "library_background.\(stale)" : "library_background_landscape.\(stale)")
            try? FileManager.default.removeItem(at: staleURL)
        }
        let url = storageDirectory.appendingPathComponent(fileName)
        try data.write(to: url, options: .atomic)
        return url.path
    }
}

/// Decides whether an imported background should be kept as an animated file
/// (preserving its bytes) or re-encoded to a static JPEG.
enum AnimatedBackgroundImport {
    /// Upper bound on the raw animated payload we are willing to keep on disk
    /// and decode per frame. Larger files fall back to a static JPEG.
    static let maxAnimatedBytes = 12 * 1024 * 1024

    static func shouldKeepAsAnimated(_ data: Data) -> Bool {
        guard data.count <= maxAnimatedBytes else { return false }
        guard let source = CGImageSourceCreateWithData(data as CFData, nil) else { return false }
        let count = CGImageSourceGetCount(source)
        guard count > 1, count <= AnimatedBackgroundLoader.maxFrames else { return false }
        // Reject upfront if any frame is too large to decode safely.
        for i in 0..<count {
            guard let cg = CGImageSourceCreateImageAtIndex(source, i, nil) else { return false }
            if CGFloat(cg.width) > AnimatedBackgroundLoader.maxFrameDimension ||
               CGFloat(cg.height) > AnimatedBackgroundLoader.maxFrameDimension {
                return false
            }
        }
        return true
    }

    /// Picks a file extension from the image type so ImageIO can reopen it.
    static func fileExtension(for data: Data) -> String {
        guard let source = CGImageSourceCreateWithData(data as CFData, nil),
              let type = CGImageSourceGetType(source) as String? else {
            return "gif"
        }
        switch type {
        case "com.compuserve.gif": return "gif"
        case "org.apng": return "apng"
        case "public.png": return "apng"
        case "org.webmproject.webp": return "webp"
        default: return "gif"
        }
    }
}
