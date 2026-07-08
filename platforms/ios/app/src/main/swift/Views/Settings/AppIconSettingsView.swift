// AppIconSettingsView.swift — Alternate app icon picker
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit

// A selectable app icon. `id` is the value passed to
// UIApplication.setAlternateIconName(_:); nil selects the primary icon.
struct AppIconOption: Identifiable, Hashable {
    let id: String?
    let displayName: String
    let previewName: String

    var isDefault: Bool { id == nil }

    // Bundled resource used when exporting a full-resolution icon image for a
    // Home Screen shortcut. Default reuses the primary marketing icon.
    var exportResourceName: String {
        isDefault ? "icon-1024" : "\(id ?? "")-export"
    }
}

extension AppIconOption {
    // Default plus the eight bundled alternate designs. The preview names point
    // at decoupled thumbnail PNGs (the primary "icon-180" for Default, and one
    // stable preview per alternate) so the picker never depends on runtime icon
    // file naming.
    static let allOptions: [AppIconOption] = [
        AppIconOption(id: nil, displayName: "Default", previewName: "icon-180"),
        AppIconOption(id: "appicon-synthwave", displayName: "Synthwave", previewName: "appicon-synthwave"),
        AppIconOption(id: "appicon-christmas", displayName: "Christmas", previewName: "appicon-christmas"),
        AppIconOption(id: "appicon-dark", displayName: "Dark", previewName: "appicon-dark"),
        AppIconOption(id: "appicon-frosted", displayName: "Frosted", previewName: "appicon-frosted"),
        AppIconOption(id: "appicon-gray", displayName: "Gray", previewName: "appicon-gray"),
        AppIconOption(id: "appicon-light", displayName: "Light", previewName: "appicon-light"),
        AppIconOption(id: "appicon-mystic", displayName: "Mystic Purple", previewName: "appicon-mystic"),
        AppIconOption(id: "appicon-purple", displayName: "Purple", previewName: "appicon-purple"),
        AppIconOption(id: "appicon-pridedark", displayName: "ARMSX2 Pride Dark", previewName: "appicon-pridedark"),
        AppIconOption(id: "appicon-pridelight", displayName: "ARMSX2 Pride Light", previewName: "appicon-pridelight")
    ]
}

// Thin @MainActor wrapper around the iOS alternate-icon API. The system stores
// the active selection, so nothing is persisted on our side.
@MainActor
enum AppIconManager {
    static var supportsAlternates: Bool {
        UIApplication.shared.supportsAlternateIcons
    }

    static var currentAlternateIconName: String? {
        UIApplication.shared.alternateIconName
    }

    // Returns the error from setAlternateIconName, or nil on success.
    // withCheckedContinuation runs its body synchronously on the main actor,
    // so wrapping the UIKit call in MainActor.assumeIsolated is safe here.
    static func setAlternateIcon(_ name: String?) async -> Error? {
        await withCheckedContinuation { continuation in
            MainActor.assumeIsolated {
                UIApplication.shared.setAlternateIconName(name) { error in
                    continuation.resume(returning: error)
                }
            }
        }
    }
}

// Best-effort detection of a host-container / LiveContainer-style install.
// Such installs relocate the app bundle under the host’s Documents/Applications
// folder; there the Home Screen icon belongs to the host, not ARMSX2, so native
// alternate-icon switching does not apply. This is advisory only — normal
// installs are never flagged, and a missed detection still falls back to the
// clearer failure alert below.
private enum AppInstallEnvironment {
    static var isLikelyExternalContainer: Bool {
        Bundle.main.bundlePath
            .range(of: "/Documents/Applications/", options: .caseInsensitive) != nil
    }
}

struct AppIconSettingsView: View {
    @State private var settings = SettingsStore.shared
    @State private var currentIcon: String? = AppIconManager.currentAlternateIconName
    @State private var showError = false
    @State private var pendingExport: AppIconOption?
    @State private var shareItem: ShareSheetItem?
    @State private var showExportError = false

    private var inExportMode: Bool { AppInstallEnvironment.isLikelyExternalContainer }

    var body: some View {
        Form {
            if inExportMode {
                Section {
                    VStack(alignment: .leading, spacing: 6) {
                        Text(settings.localized("External container install detected"))
                            .font(.subheadline.weight(.semibold))
                        Text(settings.localized("Apps running inside a host container such as LiveContainer can’t change their Home Screen icon directly — the icon belongs to the host, not ARMSX2. Export an icon and set it as your LiveContainer Home Screen shortcut icon."))
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }
                    .padding(.vertical, 2)
                }

                Section {
                    ForEach(AppIconOption.allOptions) { option in
                        appIconRow(option)
                    }
                } footer: {
                    Text(settings.localized("How to use this icon: save the exported image, then create or edit a LiveContainer Home Screen shortcut and choose it as the shortcut’s icon. The App Switcher may still show LiveContainer."))
                }
            } else if AppIconManager.supportsAlternates {
                Section {
                    ForEach(AppIconOption.allOptions) { option in
                        appIconRow(option)
                    }
                } header: {
                    Text(settings.localized("App Icon"))
                } footer: {
                    Text(settings.localized("The Home Screen icon updates after switching. iOS may take a moment to refresh."))
                }
            } else {
                Section {
                    Text(settings.localized("Alternate app icons aren’t supported on this device."))
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
        }
        .navigationTitle(settings.localized("App Icon"))
        .alert(settings.localized("Couldn’t change the app icon."), isPresented: $showError) {
            if let option = pendingExport {
                Button(settings.localized("Export Icon")) {
                    pendingExport = nil
                    exportIcon(option)
                }
            }
            Button(settings.localized("OK"), role: .cancel) { pendingExport = nil }
        } message: {
            Text(settings.localized(inExportMode
                ? "This install method can’t change the Home Screen icon directly. Export an icon and use it for a LiveContainer Home Screen shortcut instead."
                : "iOS rejected the icon change. This can happen when ARMSX2 runs inside another app’s container. The icons are bundled — you can export one instead."))
        }
        .alert(settings.localized("Couldn’t export the icon."), isPresented: $showExportError) {
            Button(settings.localized("OK"), role: .cancel) {}
        } message: {
            Text(settings.localized("The icon image couldn’t be prepared for sharing."))
        }
        .sheet(item: $shareItem) { item in
            ActivityShareSheet(activityItems: [item.url])
        }
    }

    @ViewBuilder
    private func appIconRow(_ option: AppIconOption) -> some View {
        Button {
            rowTapped(option)
        } label: {
            HStack(spacing: 14) {
                previewThumbnail(for: option)
                    .frame(width: 60, height: 60)

                Text(rowTitle(option))
                    .foregroundStyle(.primary)

                Spacer()

                trailingAccessory(for: option)
            }
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(rowTitle(option))
        .accessibilityHint(settings.localized(inExportMode
            ? "Exports this icon for a Home Screen shortcut."
            : "Sets this as the app icon."))
        .accessibilityAddTraits(currentIcon == option.id && !inExportMode ? .isSelected : [])
    }

    @ViewBuilder
    private func trailingAccessory(for option: AppIconOption) -> some View {
        if inExportMode {
            Image(systemName: "square.and.arrow.up")
                .foregroundStyle(.tint)
                .accessibilityHidden(true)
        } else if currentIcon == option.id {
            Image(systemName: "checkmark")
                .foregroundStyle(.tint)
                .accessibilityHidden(true)
        }
    }

    @ViewBuilder
    private func previewThumbnail(for option: AppIconOption) -> some View {
        if let image = previewImage(named: option.previewName) {
            Image(uiImage: image)
                .resizable()
                .scaledToFill()
                .clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
        } else {
            RoundedRectangle(cornerRadius: 14, style: .continuous)
                .fill(Color(.secondarySystemBackground))
                .overlay(
                    Image(systemName: "app")
                        .font(.title2)
                        .foregroundStyle(.secondary)
                )
        }
    }

    private func rowTitle(_ option: AppIconOption) -> String {
        option.isDefault ? settings.localized("Default") : option.displayName
    }

    private func rowTapped(_ option: AppIconOption) {
        if inExportMode {
            exportIcon(option)
        } else {
            applyIcon(option)
        }
    }

    private func applyIcon(_ option: AppIconOption) {
        // Skip when the chosen icon is already active to avoid a redundant prompt.
        guard currentIcon != option.id else { return }
        Task { @MainActor in
            if let error = await AppIconManager.setAlternateIcon(option.id) {
                NSLog("[ARMSX2 iOS AppIcon] setAlternateIconName failed: %@", error.localizedDescription)
                pendingExport = option
                showError = true
            } else {
                currentIcon = AppIconManager.currentAlternateIconName
            }
        }
    }

    private func exportIcon(_ option: AppIconOption) {
        guard let url = preparedExportURL(for: option) else {
            showExportError = true
            return
        }
        shareItem = ShareSheetItem(url: url)
    }

    // Copies the bundled full-resolution icon PNG to a temp file with a friendly
    // name so the share sheet offers a clean, savable file (Save Image / Files /
    // AirDrop). No Photos permission is requested.
    private func preparedExportURL(for option: AppIconOption) -> URL? {
        guard let source = Bundle.main.url(forResource: option.exportResourceName, withExtension: "png") else {
            return nil
        }
        let destination = FileManager.default.temporaryDirectory
            .appendingPathComponent("ARMSX2 Icon - \(option.displayName).png")
        do {
            if FileManager.default.fileExists(atPath: destination.path) {
                try FileManager.default.removeItem(at: destination)
            }
            try FileManager.default.copyItem(at: source, to: destination)
            return destination
        } catch {
            NSLog("[ARMSX2 iOS AppIcon] export prepare failed: %@", error.localizedDescription)
            return nil
        }
    }

    private func previewImage(named name: String) -> UIImage? {
        guard let url = Bundle.main.url(forResource: name, withExtension: "png") else { return nil }
        return UIImage(contentsOfFile: url.path)
    }
}
