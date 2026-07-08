// HelpView.swift — Practical user guide
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit

struct HelpSection: Identifiable {
    let id = UUID()
    let title: String
    let icon: String
    let items: [HelpItem]
}

struct HelpItem: Identifiable {
    let id = UUID()
    let question: String
    let answer: String
}

private enum HelpTopic: Hashable, Identifiable {
    case item(section: Int, item: Int)
    case about

    var id: String {
        switch self {
        case .item(let section, let item):
            return "\(section)-\(item)"
        case .about:
            return "about"
        }
    }
}

private let helpData: [HelpSection] = [
    HelpSection(title: "Settings Guide", icon: "gearshape", items: [
        HelpItem(
            question: "EE / IOP / VU0 / VU1 (JIT vs Interpreter)",
            answer: "JIT (Recompiler) is much faster. Interpreter is slower but more stable. If a game crashes or behaves incorrectly, try switching individual components to Interpreter. Changes take effect on next boot."
        ),
        HelpItem(
            question: "Fast Boot",
            answer: "Skips the PS2 BIOS intro and boots the game directly. Some games require this OFF to initialize correctly (e.g. missing 3D graphics)."
        ),
        HelpItem(
            question: "Fastmem",
            answer: "Speeds up EE memory access via direct mapping. Disable if 3D graphics are broken. Requires app restart."
        ),
        HelpItem(
            question: "MTVU",
            answer: "Offloads VU1 processing to a separate thread. Improves performance on multi-core devices, but may cause instability in some games."
        ),
        HelpItem(
            question: "Internal Resolution",
            answer: "0.25x, 0.5x, and 0.75x are performance modes for heavy games. 1x is native PS2 resolution. 2x and higher look sharper but significantly increase GPU load."
        ),
        HelpItem(
            question: "Texture packs",
            answer: "Graphics settings can load PNG/DDS replacement textures from Documents/textures/[Game Serial]/replacements/ and dump discovered textures to Documents/textures/[Game Serial]/dumps/. Dumping is for creators and can slow games down, so leave it off unless you are building a pack."
        ),
        HelpItem(
            question: "Frame Limiter",
            answer: "Keeps gameplay near the selected FPS target by changing PCSX2 Normal Speed. 60 FPS is normal NTSC timing, 30 FPS is about 50% speed, and turning it off unlocks speed for testing at the cost of heat and battery."
        ),
        HelpItem(
            question: "Patches and cheats",
            answer: "Enable GameDB patches for built-in compatibility fixes. PNACH cheats can be imported from the in-game quick menu; ARMSX2 iOS renames them to the current game's Serial_CRC.pnach and keeps // comments compatible with the core parser."
        ),
        HelpItem(
            question: "Memory cards",
            answer: "The Memory Cards settings page can create 8 MB, 16 MB, 32 MB, 64 MB, or folder memory cards, then assign them to Slot 1 or Slot 2. Slot changes apply on next boot."
        ),
        HelpItem(
            question: "RetroAchievements",
            answer: "Enable RetroAchievements from Settings, then log in with your RetroAchievements account. The status panel shows whether the client is active, which game is being tracked, achievement progress, points, leaderboards, and rich presence. Hardcore mode is core-enforced and can restrict cheats, save states, and other non-hardcore features."
        ),
        HelpItem(
            question: "VSync Queue Size",
            answer: "Number of pre-rendered frames. Higher values reduce frame drops but increase input latency. Default: 8."
        ),
    ]),
    HelpSection(title: "Overlay", icon: "speedometer", items: [
        HelpItem(
            question: "Overlay presets",
            answer: "OFF hides the overlay. Simple shows FPS, speed, CPU, and indicators. Detail adds VPS, GPU, and resolution. Full enables the Android-style diagnostic set including GS stats, settings, inputs, frame times, version, and hardware info."
        ),
        HelpItem(
            question: "In-game toggle",
            answer: "Tap the menu button (top-right) during gameplay and select Show/Hide Overlay. This toggles visibility without changing your preset."
        ),
    ]),
    HelpSection(title: "Supported Formats", icon: "doc.circle", items: [
        HelpItem(question: "Game formats", answer: "ISO, CHD, IMG, BIN, CSO, ZSO, GZ, ELF"),
        HelpItem(question: "BIOS formats", answer: "BIN, ROM (dumped from your own PS2)"),
        HelpItem(question: "Disc swapping", answer: "During gameplay, open the quick menu and choose Change Disc. ARMSX2 iOS can eject the current disc or switch to any imported ISO/CHD/IMG/BIN/CSO/ZSO/GZ disc image."),
        HelpItem(
            question: "Game covers",
            answer: "Games default to a cover grid, with a toolbar button to switch back to list view. Open the Covers menu in Games to import local images, download missing covers, or edit the online Cover Source template. Templates support ${serial}, ${title}, and ${filetitle}; the default example is https://raw.githubusercontent.com/xlenore/ps2-covers/main/covers/default/${serial}.jpg. ARMSX2 iOS scans Documents/armsx2_covers, Documents/covers, the game folder, and Documents for JPG, PNG, WebP, HEIC, or HEIF images named after the game file, file stem, game title, or serial-like names such as SLUS-20312. This also works for CHD games when metadata can be resolved."
        ),
    ]),
]

struct HelpView: View {
    @State private var settings = SettingsStore.shared
    @State private var copyStatusMessage: String?
#if targetEnvironment(macCatalyst)
    @State private var selectedTopic: HelpTopic? = .item(section: 0, item: 0)
#endif

    var body: some View {
#if targetEnvironment(macCatalyst)
        NavigationSplitView {
            List(selection: $selectedTopic) {
                ForEach(helpData.indices, id: \.self) { sectionIndex in
                    let section = helpData[sectionIndex]
                    Section {
                        ForEach(section.items.indices, id: \.self) { itemIndex in
                            let item = section.items[itemIndex]
                            Text(settings.localized(item.question))
                                .tag(HelpTopic.item(section: sectionIndex, item: itemIndex))
                        }
                    } header: {
                        Label(settings.localized(section.title), systemImage: section.icon)
                    }
                }

                Section {
                    Label(settings.localized("Version"), systemImage: "info.circle")
                        .tag(HelpTopic.about)
                } header: {
                    Text(settings.localized("About"))
                }
            }
            .navigationTitle(settings.localized("Help"))
            .listStyle(.sidebar)
        } detail: {
            helpDetail(for: selectedTopic)
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        }
        .navigationSplitViewStyle(.balanced)
#else
        NavigationStack {
            List {
                ForEach(helpData) { section in
                    Section {
                        ForEach(section.items) { item in
                            DisclosureGroup {
                                Text(settings.localized(item.answer))
                                    .font(.body)
                                    .foregroundStyle(.secondary)
                                    .padding(.vertical, 4)
                            } label: {
                                Text(settings.localized(item.question))
                                    .font(.body)
                                    .frame(maxWidth: .infinity, minHeight: 44, alignment: .leading)
                                    .contentShape(Rectangle())
                            }
                        }
                    } header: {
                        Label(settings.localized(section.title), systemImage: section.icon)
                    }
                }

                Section {
                    HStack {
                        Text(settings.localized("Version"))
                        Spacer()
                        Text(ARMSX2Bridge.buildVersion())
                            .foregroundStyle(.secondary)
                            .font(.caption)
                    }
                    Button {
                        copyTroubleshootingInfo()
                    } label: {
                        Label(settings.localized("Copy Troubleshooting Info"), systemImage: "doc.on.doc")
                    }
                    if let copyStatusMessage {
                        Text(settings.localized(copyStatusMessage))
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                } header: {
                    Label(settings.localized("About"), systemImage: "info.circle")
                }
            }
            .navigationTitle(settings.localized("Help"))
        }
#endif
    }

    @ViewBuilder
    private func helpDetail(for topic: HelpTopic?) -> some View {
        switch topic {
        case .item(let sectionIndex, let itemIndex):
            let section = helpData[sectionIndex]
            let item = section.items[itemIndex]

            ScrollView {
                VStack(alignment: .leading, spacing: 18) {
                    Label(settings.localized(section.title), systemImage: section.icon)
                        .font(.headline)
                        .foregroundStyle(.secondary)
                    Text(settings.localized(item.question))
                        .font(.largeTitle)
                        .fontWeight(.bold)
                    Text(settings.localized(item.answer))
                        .font(.body)
                        .foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
                .padding(32)
                .frame(maxWidth: .infinity, alignment: .topLeading)
            }
            .navigationTitle(settings.localized(item.question))

        case .about:
            Form {
                Section(settings.localized("App")) {
                    HStack {
                        Text(settings.localized("Version"))
                        Spacer()
                        Text(ARMSX2Bridge.buildVersion())
                            .foregroundStyle(.secondary)
                            .font(.caption)
                    }
                    Button {
                        copyTroubleshootingInfo()
                    } label: {
                        Label(settings.localized("Copy Troubleshooting Info"), systemImage: "doc.on.doc")
                    }
                    if let copyStatusMessage {
                        Text(settings.localized(copyStatusMessage))
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
            }
            .navigationTitle(settings.localized("About"))

        case .none:
            VStack(spacing: 12) {
                Image(systemName: "questionmark.circle")
                    .font(.system(size: 42))
                    .foregroundStyle(.secondary)
                Text(settings.localized("Select a help topic"))
                    .font(.title2)
                    .fontWeight(.semibold)
                    .foregroundStyle(.secondary)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
    }

    private func copyTroubleshootingInfo() {
        UIPasteboard.general.string = troubleshootingInfoText()
        copyStatusMessage = "Troubleshooting info copied."
    }

    private func troubleshootingInfoText() -> String {
        let bundle = Bundle.main
        let appName = bundle.object(forInfoDictionaryKey: "CFBundleDisplayName") as? String
            ?? bundle.object(forInfoDictionaryKey: "CFBundleName") as? String
            ?? "ARMSX2 iOS"
        let appVersion = bundle.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "Unknown"
        let buildNumber = bundle.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? "Unknown"
        let device = UIDevice.current

        return [
            "App: \(appName)",
            "Version: \(appVersion)",
            "Build: \(buildNumber)",
            "Platform: iOS",
            "iOS Version: \(device.systemVersion)",
            "Device Type: \(device.model)"
        ].joined(separator: "\n")
    }
}
