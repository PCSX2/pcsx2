// ARMSX2 macOS native SwiftUI views
// SPDX-License-Identifier: GPL-3.0+

import AppKit
import SwiftUI

private enum SidebarItem: String, CaseIterable, Identifiable {
    case games
    case bios
    case controllers
    case settings
    case storage
    case logs

    var id: String { rawValue }

    var title: String {
        switch self {
        case .games: return "Games"
        case .bios: return "BIOS"
        case .controllers: return "Controllers"
        case .settings: return "Settings"
        case .storage: return "Storage"
        case .logs: return "Logs"
        }
    }

    var icon: String {
        switch self {
        case .games: return "gamecontroller"
        case .bios: return "cpu"
        case .controllers: return "arcade.stick.console"
        case .settings: return "gearshape"
        case .storage: return "internaldrive"
        case .logs: return "doc.text.magnifyingglass"
        }
    }
}

struct MacRootView: View {
    @EnvironmentObject private var settings: MacSettingsStore
    @EnvironmentObject private var gameLibrary: GameLibrary
    @EnvironmentObject private var biosLibrary: BIOSLibrary
    @EnvironmentObject private var launcher: EmulatorLauncher
    @EnvironmentObject private var quickMenu: ARMSX2QuickMenuController
    @State private var selection: SidebarItem = .games

    var body: some View {
        VStack(spacing: 0) {
            ARMSX2DesktopToolbar(selection: $selection)
            Divider()
            Group {
                switch selection {
            case .games:
                GameLibraryView()
            case .bios:
                BIOSManagerView()
            case .controllers:
                ControllerSettingsView()
            case .settings:
                MacSettingsView()
            case .storage:
                StorageView()
            case .logs:
                LogsView()
            }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(Color(red: 0.10, green: 0.10, blue: 0.10))
        }
        .navigationTitle(MacPaths.appDisplayName)
        .onAppear {
            quickMenu.configure(settings: settings, gameLibrary: gameLibrary, biosLibrary: biosLibrary, launcher: launcher)
        }
        .onChange(of: launcher.isRunning) { _ in
            quickMenu.syncInGameAccess(settings: settings, gameLibrary: gameLibrary, biosLibrary: biosLibrary, launcher: launcher)
        }
        .onChange(of: settings.inGameQuickMenuHotkeyEnabled) { _ in
            quickMenu.syncInGameAccess(settings: settings, gameLibrary: gameLibrary, biosLibrary: biosLibrary, launcher: launcher)
        }
        .alert("Game Library", isPresented: Binding(
            get: { gameLibrary.lastMessage != nil || launcher.lastMessage != nil || biosLibrary.lastMessage != nil || settings.lastMessage != nil },
            set: {
                if !$0 {
                    gameLibrary.lastMessage = nil
                    launcher.lastMessage = nil
                    biosLibrary.lastMessage = nil
                    settings.lastMessage = nil
                }
            }
        )) {
            Button("OK") {
                gameLibrary.lastMessage = nil
                launcher.lastMessage = nil
                biosLibrary.lastMessage = nil
                settings.lastMessage = nil
            }
        } message: {
            Text(gameLibrary.lastMessage ?? launcher.lastMessage ?? biosLibrary.lastMessage ?? settings.lastMessage ?? "")
        }
    }
}

private struct ARMSX2DesktopToolbar: View {
    @EnvironmentObject private var settings: MacSettingsStore
    @EnvironmentObject private var gameLibrary: GameLibrary
    @EnvironmentObject private var launcher: EmulatorLauncher
    @Binding var selection: SidebarItem

    var body: some View {
        HStack(spacing: 0) {
            toolbarButton("Start File", "play.rectangle") {
                gameLibrary.importGames(FilePanels.chooseGames(), replaceExisting: true)
                selection = .games
            }
            toolbarButton("Games", "square.grid.3x3") { selection = .games }
            toolbarButton("BIOS", "cpu") { selection = .bios }
            toolbarButton("Start BIOS", "play") { launcher.bootBIOS(fullscreen: false) }
                .disabled(launcher.isRunning)
            toolbarButton("Controls", "arcade.stick.console") { selection = .controllers }
            toolbarButton("Refresh", "arrow.clockwise") { gameLibrary.refresh() }
            separator
            toolbarButton("Storage", "internaldrive") { selection = .storage }
            toolbarButton("Settings", "gearshape") { selection = .settings }
            toolbarButton("Logs", "doc.text.magnifyingglass") { selection = .logs }
            separator
            toolbarButton("Pause Menu", "pause.rectangle") { launcher.sendHotkey(.openPauseMenu) }
                .disabled(!launcher.isRunning)
            toolbarButton("OSD", "text.bubble") {
                settings.ensureUsableOSDPreset()
                launcher.sendHotkey(.toggleOSD)
            }
            .disabled(!launcher.isRunning)
            toolbarButton("Stop", "power") { launcher.stop() }
                .disabled(!launcher.isRunning)
            Spacer()
            Text(MacPaths.appDisplayName)
                .font(.caption)
                .fontWeight(.semibold)
                .foregroundStyle(.white.opacity(0.78))
                .padding(.trailing, 12)
        }
        .frame(height: 54)
        .background(Color(red: 0.13, green: 0.13, blue: 0.13))
    }

    private var separator: some View {
        Rectangle()
            .fill(.white.opacity(0.12))
            .frame(width: 1, height: 34)
            .padding(.horizontal, 8)
    }

    private func toolbarButton(_ title: String, _ systemImage: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            VStack(spacing: 3) {
                Image(systemName: systemImage)
                    .font(.system(size: 18, weight: .medium))
                Text(title)
                    .font(.caption2)
            }
            .frame(width: 70, height: 50)
            .foregroundStyle(.white.opacity(0.9))
        }
        .buttonStyle(.plain)
        .help(title)
    }
}

@MainActor
final class ARMSX2QuickMenuController: ObservableObject {
    private var panel: NSPanel?
    private var localHotkeyMonitor: Any?
    private var globalHotkeyMonitor: Any?
    private weak var settings: MacSettingsStore?
    private weak var gameLibrary: GameLibrary?
    private weak var biosLibrary: BIOSLibrary?
    private weak var launcher: EmulatorLauncher?

    deinit {
        if let localHotkeyMonitor {
            NSEvent.removeMonitor(localHotkeyMonitor)
        }
        if let globalHotkeyMonitor {
            NSEvent.removeMonitor(globalHotkeyMonitor)
        }
    }

    func configure(
        settings: MacSettingsStore,
        gameLibrary: GameLibrary,
        biosLibrary: BIOSLibrary,
        launcher: EmulatorLauncher
    ) {
        self.settings = settings
        self.gameLibrary = gameLibrary
        self.biosLibrary = biosLibrary
        self.launcher = launcher
        installHotkeyMonitorsIfNeeded()
        syncInGameAccess(settings: settings, gameLibrary: gameLibrary, biosLibrary: biosLibrary, launcher: launcher)
    }

    func syncInGameAccess(
        settings: MacSettingsStore,
        gameLibrary: GameLibrary,
        biosLibrary: BIOSLibrary,
        launcher: EmulatorLauncher
    ) {
        self.settings = settings
        self.gameLibrary = gameLibrary
        self.biosLibrary = biosLibrary
        self.launcher = launcher
        installHotkeyMonitorsIfNeeded()
    }

    func show(
        settings: MacSettingsStore,
        gameLibrary: GameLibrary,
        biosLibrary: BIOSLibrary,
        launcher: EmulatorLauncher
    ) {
        let content = ARMSX2QuickSettingsPanelView()
            .environmentObject(settings)
            .environmentObject(gameLibrary)
            .environmentObject(biosLibrary)
            .environmentObject(launcher)

        if let panel {
            panel.contentViewController = NSHostingController(rootView: content)
            panel.makeKeyAndOrderFront(nil)
            NSApp.activate(ignoringOtherApps: true)
            return
        }

        let panel = NSPanel(
            contentRect: NSRect(x: 0, y: 0, width: 470, height: 650),
            styleMask: [.titled, .closable, .resizable, .utilityWindow],
            backing: .buffered,
            defer: false
        )
        panel.title = "ARMSX2 In-Game Menu"
        panel.contentMinSize = NSSize(width: 430, height: 520)
        panel.level = .floating
        panel.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary]
        panel.isReleasedWhenClosed = false
        panel.contentViewController = NSHostingController(rootView: content)
        panel.center()
        self.panel = panel

        panel.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    private func installHotkeyMonitorsIfNeeded() {
        if localHotkeyMonitor == nil {
            localHotkeyMonitor = NSEvent.addLocalMonitorForEvents(matching: .keyDown) { [weak self] event in
                guard let self else { return event }
                return self.handleQuickMenuHotkey(event) ? nil : event
            }
        }

        if globalHotkeyMonitor == nil {
            globalHotkeyMonitor = NSEvent.addGlobalMonitorForEvents(matching: .keyDown) { [weak self] event in
                guard let self else { return }
                _ = self.handleQuickMenuHotkey(event)
            }
        }
    }

    private func handleQuickMenuHotkey(_ event: NSEvent) -> Bool {
        guard settings?.inGameQuickMenuHotkeyEnabled == true,
              event.keyCode == 46 else {
            return false
        }

        let flags = event.modifierFlags.intersection(.deviceIndependentFlagsMask)
        guard flags.contains(.command), flags.contains(.shift), !flags.contains(.option), !flags.contains(.control),
              let settings, let gameLibrary, let biosLibrary, let launcher else {
            return false
        }

        show(settings: settings, gameLibrary: gameLibrary, biosLibrary: biosLibrary, launcher: launcher)
        return true
    }
}

private struct ARMSX2QuickSettingsPanelView: View {
    @EnvironmentObject private var settings: MacSettingsStore
    @EnvironmentObject private var gameLibrary: GameLibrary
    @EnvironmentObject private var biosLibrary: BIOSLibrary
    @EnvironmentObject private var launcher: EmulatorLauncher

    var body: some View {
        VStack(spacing: 0) {
            HStack {
                VStack(alignment: .leading, spacing: 3) {
                    Text("ARMSX2 In-Game Menu")
                        .font(.headline)
                    Text(launcher.isRunning ? "Game running | Cmd+Shift+M" : "No game running | Cmd+Shift+M")
                        .font(.caption)
                        .foregroundStyle(launcher.isRunning ? .green : .secondary)
                }
                Spacer()
                Button {
                    settings.ensureUsableOSDPreset()
                    launcher.sendHotkey(.toggleOSD)
                } label: {
                    Label("OSD", systemImage: "text.bubble")
                }
                .disabled(!launcher.isRunning)
                Button(role: .destructive) {
                    launcher.stop()
                } label: {
                    Label("Stop", systemImage: "power")
                }
                .disabled(!launcher.isRunning)
            }
            .padding(16)

            Divider()

            ScrollView {
                VStack(alignment: .leading, spacing: 14) {
                    quickSection("Access", systemImage: "keyboard") {
                        Toggle("Cmd+Shift+M Hotkey", isOn: $settings.inGameQuickMenuHotkeyEnabled)
                        settingNote("Cmd+Shift+M opens ARMSX2 wrapper controls while the game window is focused. PCSX2 Menu below opens the emulator backend menu.")
                    }

                    quickSection("PCSX2 Runtime", systemImage: "gamecontroller") {
                        HStack {
                            quickButton("PCSX2 Menu", "pause.rectangle") {
                                launcher.sendHotkey(.openPauseMenu)
                            }
                            quickButton("Pause", "pause.fill") {
                                launcher.sendHotkey(.togglePause)
                            }
                            quickButton("Screenshot", "camera") {
                                launcher.sendHotkey(.screenshot)
                            }
                        }
                        .disabled(!launcher.isRunning)

                        HStack {
                            quickButton("Save State", "tray.and.arrow.down") {
                                launcher.sendHotkey(.saveState)
                            }
                            quickButton("Load State", "tray.and.arrow.up") {
                                launcher.sendHotkey(.loadState)
                            }
                            quickButton("Frame Limit", "speedometer") {
                                launcher.sendHotkey(.toggleFrameLimit)
                            }
                        }
                        .disabled(!launcher.isRunning)
                        settingNote("These buttons send hotkeys to the running PCSX2 backend. Use PCSX2 Menu for backend-only emulator options.")
                    }

                    quickSection("ARMSX2 OSD", systemImage: "text.alignleft") {
                        Picker("Preset", selection: $settings.osdPreset) {
                            Text("Off").tag(0)
                            Text("Simple").tag(1)
                            Text("Detail").tag(2)
                            Text("Full").tag(3)
                        }
                        .pickerStyle(.segmented)
                        Toggle("Show Texture Status", isOn: $settings.showTextureReplacementStatus)
                        settingNote("Off hides the overlay. Simple shows the core timing line. Detail and Full add diagnostic data such as resolution, GS stats, hardware info, and frame times.")
                    }

                    quickSection("Core", systemImage: "cpu") {
                        Picker("EE Core", selection: $settings.eeCoreType) {
                            Text("Interpreter").tag(1)
                            Text("ARM64 JIT").tag(2)
                        }
                        .pickerStyle(.segmented)
                        Toggle("Fast Boot", isOn: $settings.fastBoot)
                        Toggle("Frame Limiter", isOn: $settings.frameLimiterEnabled)
                        Toggle("GameDB Core Fixes", isOn: $settings.enableGameFixes)
                        Toggle("GameDB Hardware Fixes", isOn: $settings.enableGameDBHardwareFixes)
                        settingNote("JIT is faster; Interpreter is slower but can help isolate crashes. Fast Boot skips the BIOS intro. GameDB applies built-in compatibility fixes.")
                    }

                    quickSection("Graphics", systemImage: "display") {
                        Picker("Renderer", selection: $settings.renderer) {
                            Text("Metal").tag(17)
                            Text("Software").tag(13)
                        }
                        .pickerStyle(.segmented)
                        Stepper(value: $settings.upscale, in: 1...8, step: 1) {
                            Text("Upscale: \(Int(settings.upscale))x")
                        }
                        Toggle("Hardware Mipmapping", isOn: $settings.hardwareMipmapping)
                        Toggle("FXAA", isOn: $settings.fxaa)
                        settingNote("Metal is the intended renderer on macOS. Higher upscale values look sharper but increase GPU load.")
                    }

                    quickSection("Textures", systemImage: "photo.on.rectangle") {
                        Toggle("Load Replacements", isOn: $settings.loadTextureReplacements)
                        Toggle("Async Loading", isOn: $settings.asyncTextureReplacements)
                            .disabled(!settings.loadTextureReplacements)
                        Toggle("Precache Replacements", isOn: $settings.precacheTextureReplacements)
                            .disabled(!settings.loadTextureReplacements)
                        Toggle("Dump Replaceable Textures", isOn: $settings.dumpReplaceableTextures)
                        HStack {
                            Button {
                                settings.importTexturePacks(FilePanels.chooseTexturePacks())
                            } label: {
                                Label("Import Pack", systemImage: "square.and.arrow.down")
                            }
                            Button {
                                settings.openTextureFolder()
                            } label: {
                                Label("Open Folder", systemImage: "folder")
                            }
                        }
                        settingNote("Texture packs load from the textures folder. Dumping is for pack creation and can slow games down, so leave it off unless you are building a pack.")
                    }

                    quickSection("Library", systemImage: "square.grid.3x3") {
                        HStack {
                            Button {
                                gameLibrary.refresh()
                            } label: {
                                Label("Refresh", systemImage: "arrow.clockwise")
                            }
                            Button {
                                Task { await gameLibrary.downloadMissingCovers(template: settings.coverTemplate) }
                            } label: {
                                Label("Covers", systemImage: "square.and.arrow.down")
                            }
                            Button {
                                launcher.bootBIOS(fullscreen: false)
                            } label: {
                                Label("Start BIOS", systemImage: "play")
                            }
                            .disabled(launcher.isRunning)
                        }

                        HStack {
                            Button {
                                settings.refreshControllers()
                            } label: {
                                Label("Refresh Controllers", systemImage: "gamecontroller")
                            }
                            Text(controllerSummary)
                                .font(.caption)
                                .foregroundStyle(.secondary)
                        }
                        settingNote("Library actions stay in ARMSX2. Start BIOS launches a no-disc BIOS boot through the bundled backend.")
                    }
                }
                .padding(16)
            }
        }
        .frame(minWidth: 430, minHeight: 520)
    }

    private var controllerSummary: String {
        if settings.detectedControllerNames.isEmpty {
            return "No controllers detected"
        }
        return "\(settings.detectedControllerNames.count) controller\(settings.detectedControllerNames.count == 1 ? "" : "s")"
    }

    private func quickButton(_ title: String, _ systemImage: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Label(title, systemImage: systemImage)
                .frame(maxWidth: .infinity)
        }
    }

    private func settingNote(_ text: String) -> some View {
        Text(text)
            .font(.caption)
            .foregroundStyle(.secondary)
            .fixedSize(horizontal: false, vertical: true)
    }

    private func quickSection<Content: View>(
        _ title: String,
        systemImage: String,
        @ViewBuilder content: () -> Content
    ) -> some View {
        GroupBox {
            VStack(alignment: .leading, spacing: 10) {
                content()
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        } label: {
            Label(title, systemImage: systemImage)
                .font(.subheadline.weight(.semibold))
        }
    }
}

struct GameLibraryView: View {
    @EnvironmentObject private var settings: MacSettingsStore
    @EnvironmentObject private var gameLibrary: GameLibrary
    @EnvironmentObject private var launcher: EmulatorLauncher
    @State private var selectedGame: GameEntry.ID?
    @State private var showList = false
    @State private var fullscreenOnBoot = false

    private let columns = [
        GridItem(.adaptive(minimum: 116, maximum: 132), spacing: 14),
    ]

    var body: some View {
        VStack(spacing: 0) {
            toolbar
            Divider()

            if gameLibrary.games.isEmpty {
                EmptyGameLibraryView {
                    gameLibrary.importGames(FilePanels.chooseGames(), replaceExisting: true)
                }
            } else if showList {
                gameTable
            } else {
                ScrollView {
                    LazyVGrid(columns: columns, spacing: 18) {
                        ForEach(gameLibrary.games) { game in
                            GameCoverTile(
                                game: game,
                                selected: selectedGame == game.id,
                                boot: { boot(game) },
                                importCover: { importCover(for: game) },
                                downloadCover: { Task { await gameLibrary.downloadCover(for: game, template: settings.coverTemplate) } }
                            )
                            .onTapGesture {
                                selectedGame = game.id
                            }
                        }
                    }
                    .padding(12)
                }
                .background(Color(red: 0.08, green: 0.08, blue: 0.08))
            }
        }
        .navigationTitle("Games")
    }

    private var toolbar: some View {
        HStack(spacing: 10) {
            Button {
                gameLibrary.importGames(FilePanels.chooseGames(), replaceExisting: true)
            } label: {
                Label("Import", systemImage: "plus")
            }

            Button {
                gameLibrary.refresh()
            } label: {
                Label("Refresh", systemImage: "arrow.clockwise")
            }

            Button {
                Task { await gameLibrary.downloadMissingCovers(template: settings.coverTemplate) }
            } label: {
                Label("Download Covers", systemImage: "square.and.arrow.down")
            }

            Button {
                if let folder = FilePanels.chooseFolder(title: "Add External Game Folder") {
                    gameLibrary.addExternalFolder(folder)
                }
            } label: {
                Label("Add Folder", systemImage: "folder.badge.plus")
            }

            Toggle("Fullscreen", isOn: $fullscreenOnBoot)
                .toggleStyle(.checkbox)

            Spacer()

            Text("\(gameLibrary.games.count) game\(gameLibrary.games.count == 1 ? "" : "s")")
                .font(.caption)
                .foregroundStyle(.secondary)

            Picker("View", selection: $showList) {
                Label("Grid", systemImage: "square.grid.2x2").tag(false)
                Label("List", systemImage: "list.bullet").tag(true)
            }
            .pickerStyle(.segmented)
            .frame(width: 128)
        }
        .padding(.horizontal, 14)
        .padding(.vertical, 10)
        .background(Color(nsColor: .windowBackgroundColor))
    }

    private var gameTable: some View {
        Table(gameLibrary.games, selection: $selectedGame) {
            TableColumn("Game") { game in
                HStack(spacing: 10) {
                    CoverArtView(url: game.coverURL)
                        .frame(width: 34, height: 50)
                    VStack(alignment: .leading) {
                        Text(game.displayName)
                        Text(game.name)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }
            }
            TableColumn("Source", value: \.source)
            TableColumn("Size", value: \.sizeText)
            TableColumn("Serial") { game in
                Text(game.serial ?? "")
                    .foregroundStyle(.secondary)
            }
            TableColumn("Region") { game in
                Text(game.region ?? "")
                    .foregroundStyle(.secondary)
            }
            TableColumn("Actions") { game in
                HStack {
                    Button("Cover") { importCover(for: game) }
                }
            }
        }
        .onTapGesture(count: 2) {
            if let selectedGame,
               let game = gameLibrary.games.first(where: { $0.id == selectedGame }) {
                boot(game)
            }
        }
    }

    private func boot(_ game: GameEntry) {
        settings.ini.set("GameISO", "BootISO", game.url.path)
        launcher.boot(game, fastBoot: settings.fastBoot, fullscreen: fullscreenOnBoot)
    }

    private func importCover(for game: GameEntry) {
        guard let url = FilePanels.chooseCover() else { return }
        gameLibrary.importCover(url, for: game)
    }
}

private struct EmptyGameLibraryView: View {
    let importGames: () -> Void

    var body: some View {
        VStack(spacing: 12) {
            Image(systemName: "opticaldisc")
                .font(.system(size: 48))
                .foregroundStyle(.secondary)
            Text("No Games Found")
                .font(.title2)
                .fontWeight(.semibold)
            Text("Import PS2 disc images or add an external game folder.")
                .foregroundStyle(.secondary)
            Button {
                importGames()
            } label: {
                Label("Import Games", systemImage: "plus")
            }
            .buttonStyle(.borderedProminent)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .padding()
    }
}

private struct GameCoverTile: View {
    let game: GameEntry
    let selected: Bool
    let boot: () -> Void
    let importCover: () -> Void
    let downloadCover: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            CoverArtView(url: game.coverURL)
                .frame(width: 108, height: 154)
                .shadow(color: .black.opacity(0.55), radius: 5, y: 2)

            Text(game.displayName)
                .font(.caption2)
                .lineLimit(1)
                .foregroundStyle(.white.opacity(0.88))
                .frame(width: 108)
        }
        .padding(5)
        .background(selected ? Color.accentColor.opacity(0.28) : Color.clear)
        .clipShape(RoundedRectangle(cornerRadius: 6))
        .contentShape(Rectangle())
        .onTapGesture(count: 2) {
            boot()
        }
        .contextMenu {
            Button("Boot", action: boot)
            Button("Import Cover", action: importCover)
            Button("Download Cover", action: downloadCover)
            Button("Reveal in Finder") {
                NSWorkspace.shared.activateFileViewerSelecting([game.url])
            }
        }
    }
}

private struct CoverArtView: View {
    let url: URL?

    var body: some View {
        ZStack {
            Rectangle()
                .fill(Color.black)
            if let image = image {
                Image(nsImage: image)
                    .resizable()
                    .scaledToFill()
                    .clipped()
            }
        }
        .overlay {
            Rectangle()
                .strokeBorder(Color.white.opacity(0.12))
        }
    }

    private var image: NSImage? {
        guard let url else { return nil }
        return NSImage(contentsOf: url)
    }
}

struct BIOSManagerView: View {
    @EnvironmentObject private var biosLibrary: BIOSLibrary
    @EnvironmentObject private var launcher: EmulatorLauncher
    @State private var fullscreenOnBoot = false

    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Button {
                    biosLibrary.importBIOS(FilePanels.chooseBIOS())
                } label: {
                    Label("Import BIOS", systemImage: "plus")
                }
                Button {
                    biosLibrary.refresh()
                } label: {
                    Label("Refresh", systemImage: "arrow.clockwise")
                }
                Button {
                    launcher.bootBIOS(fullscreen: fullscreenOnBoot)
                } label: {
                    Label("Start BIOS", systemImage: "play")
                }
                .disabled(launcher.isRunning)
                Toggle("Fullscreen", isOn: $fullscreenOnBoot)
                    .toggleStyle(.checkbox)
                Spacer()
            }
            .padding()
            Divider()

            List(biosLibrary.biosFiles, id: \.path) { bios in
                HStack(spacing: 14) {
                    let region = BIOSRegionInfo.detect(from: bios)
                    BIOSRegionBadge(region: region)
                    VStack(alignment: .leading) {
                        Text(bios.lastPathComponent)
                        Text("\(region.title) | \(bios.path)")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                    Spacer()
                    if bios.lastPathComponent == biosLibrary.selectedBIOS {
                        Image(systemName: "checkmark.circle.fill")
                            .foregroundStyle(.green)
                    }
                    Button("Use") {
                        biosLibrary.setDefault(bios)
                    }
                }
                .padding(.vertical, 4)
            }
        }
        .navigationTitle("BIOS")
    }
}

private struct BIOSRegionBadge: View {
    let region: BIOSRegionInfo

    var body: some View {
        ZStack {
            RoundedRectangle(cornerRadius: 4)
                .fill(Color.black.opacity(0.88))
            if let image = flagImage {
                Image(nsImage: image)
                    .resizable()
                    .scaledToFit()
                    .padding(4)
            } else {
                Text(region.code)
                    .font(.system(size: 12, weight: .bold, design: .rounded))
                    .foregroundStyle(.white)
            }
        }
        .frame(width: 48, height: 32)
        .overlay {
            RoundedRectangle(cornerRadius: 4)
                .strokeBorder(Color.white.opacity(0.16))
        }
    }

    private var flagImage: NSImage? {
        for url in flagCandidates {
            if let image = NSImage(contentsOf: url) {
                return image
            }
        }
        return nil
    }

    private var flagCandidates: [URL] {
        let name = "\(region.flagAssetName).svg"
        let bundle = Bundle.main.resourceURL
        let current = URL(fileURLWithPath: FileManager.default.currentDirectoryPath, isDirectory: true)
        let likelyWorktree = Bundle.main.bundleURL
            .deletingLastPathComponent()
            .deletingLastPathComponent()
        return [
            bundle?.appendingPathComponent("icons/flags/\(name)"),
            bundle?.appendingPathComponent("resources/icons/flags/\(name)"),
            current.appendingPathComponent("bin/resources/icons/flags/\(name)"),
            likelyWorktree.appendingPathComponent("bin/resources/icons/flags/\(name)"),
        ].compactMap { $0 }
    }
}

struct ControllerSettingsView: View {
    @EnvironmentObject private var settings: MacSettingsStore

    var body: some View {
        Form {
            Section("Input Sources") {
                Toggle("SDL Controllers", isOn: $settings.sdlInputEnabled)
                Toggle("Enhanced Controller Mode", isOn: $settings.sdlEnhancedMode)
                Toggle("PlayStation LED", isOn: $settings.sdlPlayerLED)
                Picker("Layout", selection: $settings.controllerProfileMode) {
                    ForEach(ControllerProfileMode.allCases) { mode in
                        Text(mode.title).tag(mode)
                    }
                }
                .pickerStyle(.segmented)
                HStack {
                    Button {
                        settings.refreshControllers()
                    } label: {
                        Label("Refresh Controllers", systemImage: "arrow.clockwise")
                    }
                    Button {
                        settings.applyStandardControllerMappings()
                    } label: {
                        Label("Apply Mapping", systemImage: "checkmark.circle")
                    }
                    Button {
                        settings.resetControllerDefaults()
                    } label: {
                        Label("Defaults", systemImage: "arrow.uturn.backward")
                    }
                    Spacer()
                    Text(settings.controllerStatus)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }

            Section("Detected Controllers") {
                if settings.detectedControllerNames.isEmpty {
                    Text("No controllers detected")
                        .foregroundStyle(.secondary)
                } else {
                    ForEach(Array(settings.detectedControllerNames.enumerated()), id: \.offset) { index, name in
                        HStack {
                            Image(systemName: "gamecontroller")
                            Text("SDL-\(index)")
                            Text(name)
                                .foregroundStyle(.secondary)
                        }
                    }
                }
            }

            Section("Players") {
                ForEach($settings.controllerPorts) { $port in
                    ControllerPortEditor(
                        port: $port,
                        detectedControllerNames: settings.detectedControllerNames,
                        mode: settings.controllerProfileMode
                    )
                    .padding(.vertical, 6)
                }
            }
        }
        .formStyle(.grouped)
        .navigationTitle("Controllers")
        .padding(.top, 8)
        .onAppear {
            settings.refreshControllers()
        }
    }
}

private struct ControllerPortEditor: View {
    @Binding var port: ControllerPortConfig
    let detectedControllerNames: [String]
    let mode: ControllerProfileMode

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Toggle("Player \(port.playerNumber)", isOn: $port.enabled)
                Spacer()
                Text(padSlotText)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            HStack(spacing: 12) {
                Picker("Device", selection: $port.deviceIndex) {
                    ForEach(0..<8, id: \.self) { index in
                        Text(deviceName(for: index)).tag(index)
                    }
                }
                Picker("Type", selection: $port.controllerType) {
                    Text("DualShock 2").tag("DualShock2")
                    Text("Guitar").tag("Guitar")
                    Text("Jogcon").tag("Jogcon")
                    Text("NeGcon").tag("NeGcon")
                    Text("Pop'n").tag("Popn")
                }
            }
            .disabled(!port.enabled)

            Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 8) {
                controllerSlider("Deadzone", value: $port.deadzone, range: 0...1)
                controllerSlider("Sensitivity", value: $port.axisScale, range: 0.01...2)
                controllerSlider("Button Deadzone", value: $port.buttonDeadzone, range: 0...1)
                controllerSlider("Pressure", value: $port.pressureModifier, range: 0.01...1)
                controllerSlider("Large Motor", value: $port.largeMotorScale, range: 0...2)
                controllerSlider("Small Motor", value: $port.smallMotorScale, range: 0...2)
            }
            .disabled(!port.enabled)

            HStack(spacing: 12) {
                Picker("Left Stick", selection: $port.invertLeft) {
                    Text("Normal").tag(0)
                    Text("Invert X").tag(1)
                    Text("Invert Y").tag(2)
                    Text("Invert X/Y").tag(3)
                }
                Picker("Right Stick", selection: $port.invertRight) {
                    Text("Normal").tag(0)
                    Text("Invert X").tag(1)
                    Text("Invert Y").tag(2)
                    Text("Invert X/Y").tag(3)
                }
            }
            .disabled(!port.enabled)
        }
    }

    private var padSlotText: String {
        if mode == .port1Multitap {
            return ["Pad1 / 1A", "Pad3 / 1B", "Pad4 / 1C", "Pad5 / 1D"][port.id]
        }
        return ["Pad1 / 1A", "Pad2 / 2A", "Pad3 / 1B", "Pad4 / 1C"][port.id]
    }

    private func deviceName(for index: Int) -> String {
        if detectedControllerNames.indices.contains(index) {
            return "SDL-\(index) \(detectedControllerNames[index])"
        }
        return "SDL-\(index)"
    }

    private func controllerSlider(_ title: String, value: Binding<Double>, range: ClosedRange<Double>) -> some View {
        GridRow {
            Text(title)
                .frame(width: 112, alignment: .leading)
            Slider(value: value, in: range)
                .frame(minWidth: 190)
            Text(String(format: "%.2f", value.wrappedValue))
                .font(.system(.caption, design: .monospaced))
                .foregroundStyle(.secondary)
                .frame(width: 44, alignment: .trailing)
        }
    }
}

struct MacSettingsView: View {
    @EnvironmentObject private var settings: MacSettingsStore
    @EnvironmentObject private var gameLibrary: GameLibrary
    @EnvironmentObject private var launcher: EmulatorLauncher

    var body: some View {
        Form {
            Section("Core") {
                Picker("EE Core", selection: $settings.eeCoreType) {
                    Text("Interpreter").tag(1)
                    Text("ARM64 JIT").tag(2)
                }
                Toggle("IOP JIT", isOn: $settings.iopJIT)
                Toggle("VU0 JIT", isOn: $settings.vu0JIT)
                Toggle("VU1 JIT", isOn: $settings.vu1JIT)
                Toggle("Fast Boot", isOn: $settings.fastBoot)
                Toggle("Fastmem", isOn: $settings.fastmem)
                Toggle("MTVU", isOn: $settings.mtvu)
                Toggle("Frame Limiter", isOn: $settings.frameLimiterEnabled)
                settingDescription("JIT is much faster; Interpreter is slower but useful for stability testing. Fast Boot skips the PS2 BIOS intro and changes apply on next boot.")
            }

            Section("GameDB and Patches") {
                Toggle("GameDB Core Fixes", isOn: $settings.enableGameFixes)
                Toggle("GameDB Hardware Fixes", isOn: $settings.enableGameDBHardwareFixes)
                Toggle("Enable PNACH Patches", isOn: $settings.enablePatches)
                Toggle("Enable Cheats", isOn: $settings.enableCheats)
                Toggle("Widescreen Patches", isOn: $settings.widescreenPatches)
                Toggle("No-Interlacing Patches", isOn: $settings.noInterlacingPatches)
                Toggle("Host Filesystem", isOn: $settings.hostFilesystem)
                settingDescription("GameDB applies built-in compatibility fixes. PNACH patches and cheats are loaded through the core and are best tested per game.")
            }

            Section("Graphics") {
                Picker("Renderer", selection: $settings.renderer) {
                    Text("Metal").tag(17)
                    Text("Software").tag(13)
                }
                Picker("Aspect Ratio", selection: $settings.aspectRatio) {
                    Text("Auto").tag("Auto 4:3/3:2")
                    Text("4:3").tag("4:3")
                    Text("16:9").tag("16:9")
                    Text("Stretch").tag("Stretch")
                }
                Stepper(value: $settings.upscale, in: 1...8, step: 1) {
                    Text("Upscale: \(Int(settings.upscale))x")
                }
                Picker("Texture Filtering", selection: $settings.textureFiltering) {
                    Text("Nearest").tag(0)
                    Text("Bilinear").tag(1)
                    Text("Bilinear Forced").tag(2)
                }
                Toggle("Hardware Mipmapping", isOn: $settings.hardwareMipmapping)
                Toggle("FXAA", isOn: $settings.fxaa)
                Picker("CAS", selection: $settings.casMode) {
                    Text("Off").tag(0)
                    Text("Sharpen Only").tag(1)
                    Text("Sharpen + Resize").tag(2)
                }
                Picker("Deinterlace", selection: $settings.interlaceMode) {
                    Text("Automatic").tag(7)
                    Text("None").tag(0)
                    Text("Bob").tag(1)
                    Text("Blend").tag(2)
                }
                Picker("Blending Accuracy", selection: $settings.blendingAccuracy) {
                    Text("Basic").tag(0)
                    Text("Medium").tag(1)
                    Text("High").tag(2)
                    Text("Full").tag(3)
                }
                Picker("Trilinear Filtering", selection: $settings.trilinearFiltering) {
                    Text("Automatic").tag(1)
                    Text("Off").tag(0)
                    Text("Forced").tag(2)
                }
                settingDescription("Metal is the preferred macOS renderer. Higher internal resolution looks sharper but significantly increases GPU load.")
            }

            Section("Hardware Hacks") {
                Stepper(value: $settings.halfPixelOffset, in: 0...4) {
                    Text("Half Pixel Offset: \(settings.halfPixelOffset)")
                }
                Stepper(value: $settings.roundSprite, in: 0...4) {
                    Text("Round Sprite: \(settings.roundSprite)")
                }
                Toggle("Align Sprite", isOn: $settings.alignSprite)
                Toggle("Merge Sprite", isOn: $settings.mergeSprite)
                Toggle("Wild Arms Offset", isOn: $settings.wildArmsOffset)
                Stepper(value: $settings.textureOffsetX, in: -4096...4096) {
                    Text("Texture Offset X: \(settings.textureOffsetX)")
                }
                Stepper(value: $settings.textureOffsetY, in: -4096...4096) {
                    Text("Texture Offset Y: \(settings.textureOffsetY)")
                }
                Stepper(value: $settings.skipDrawStart, in: 0...5000) {
                    Text("Skipdraw Start: \(settings.skipDrawStart)")
                }
                Stepper(value: $settings.skipDrawEnd, in: 0...5000) {
                    Text("Skipdraw End: \(settings.skipDrawEnd)")
                }
                settingDescription("Hardware hacks are compatibility tools for specific games. Leave them at defaults unless a title needs a known workaround.")
            }

            Section("Texture Replacement") {
                Toggle("Load Replacements", isOn: $settings.loadTextureReplacements)
                Toggle("Async Replacement Loading", isOn: $settings.asyncTextureReplacements)
                    .disabled(!settings.loadTextureReplacements)
                Toggle("Precache Replacements", isOn: $settings.precacheTextureReplacements)
                    .disabled(!settings.loadTextureReplacements)
                Toggle("Dump Replaceable Textures", isOn: $settings.dumpReplaceableTextures)
                Toggle("Dump Mipmaps", isOn: $settings.dumpReplaceableMipmaps)
                    .disabled(!settings.dumpReplaceableTextures)
                Toggle("Dump During FMV", isOn: $settings.dumpTexturesWithFMVActive)
                    .disabled(!settings.dumpReplaceableTextures)
                Toggle("Show Texture Status", isOn: $settings.showTextureReplacementStatus)
                HStack {
                    Button {
                        settings.importTexturePacks(FilePanels.chooseTexturePacks())
                    } label: {
                        Label("Import Pack", systemImage: "square.and.arrow.down")
                    }
                    Button {
                        settings.openTextureFolder()
                    } label: {
                        Label("Open Folder", systemImage: "folder")
                    }
                    Spacer()
                }
                settingDescription("Texture packs load from the textures folder. Dumping writes discovered textures for pack creation and can slow games down.")
            }

            Section("Speed Hacks") {
                Toggle("Fast CDVD", isOn: $settings.fastCDVD)
                Toggle("VU1 Instant", isOn: $settings.vu1Instant)
                Toggle("Wait Loop Detection", isOn: $settings.waitLoop)
                Toggle("INTC Spin Detection", isOn: $settings.intcStat)
                Stepper(value: $settings.eeCycleRate, in: -3...3) {
                    Text("EE Cycle Rate: \(settings.eeCycleRate)")
                }
                settingDescription("Speed hacks can improve performance but may break timing-sensitive games. Test one change at a time when debugging.")
            }

            Section("OSD") {
                Picker("Preset", selection: $settings.osdPreset) {
                    Text("Off").tag(0)
                    Text("Simple").tag(1)
                    Text("Detail").tag(2)
                    Text("Full").tag(3)
                }
                Toggle("Cmd+Shift+M ARMSX2 Menu Hotkey", isOn: $settings.inGameQuickMenuHotkeyEnabled)
                settingDescription("The OSD preset controls the performance overlay. Cmd+Shift+M opens ARMSX2's in-game menu; PCSX2's pause menu remains separate.")
            }

            Section("RetroAchievements and Network") {
                Text(settings.retroAchievementsStatus)
                    .font(.caption)
                    .foregroundStyle(.secondary)
                TextField("Username", text: $settings.retroAchievementsUsername)
                SecureField("Password", text: $settings.retroAchievementsPassword)
                HStack {
                    Button("Login") {
                        Task { await settings.loginRetroAchievements() }
                    }
                    .disabled(settings.retroAchievementsLoginInProgress)
                    if settings.retroAchievementsLoginInProgress {
                        ProgressView()
                            .controlSize(.small)
                    }
                    Spacer()
                    Button("Logout") {
                        settings.logoutRetroAchievements()
                    }
                }
                SecureField("Login Token", text: $settings.retroAchievementsToken)
                Button("Save Token") {
                    settings.saveRetroAchievementsToken()
                }
                Toggle("RetroAchievements", isOn: $settings.achievementsEnabled)
                Toggle("Hardcore Mode", isOn: $settings.achievementsHardcore)
                Toggle("Achievement Notifications", isOn: $settings.achievementsNotifications)
                Toggle("Leaderboard Notifications", isOn: $settings.achievementsLeaderboardNotifications)
                Toggle("Sound Effects", isOn: $settings.achievementsSoundEffects)
                Toggle("In-Game Overlays", isOn: $settings.achievementsOverlays)
                Toggle("Leaderboard Overlays", isOn: $settings.achievementsLeaderboardOverlays)
                Toggle("Encore Mode", isOn: $settings.achievementsEncore)
                Toggle("Spectator Mode", isOn: $settings.achievementsSpectator)
                Toggle("Unofficial Achievements", isOn: $settings.achievementsUnofficial)
                Stepper(value: $settings.achievementsNotificationsDuration, in: 1...60) {
                    Text("Achievement Popup: \(settings.achievementsNotificationsDuration)s")
                }
                Stepper(value: $settings.achievementsLeaderboardsDuration, in: 1...60) {
                    Text("Leaderboard Popup: \(settings.achievementsLeaderboardsDuration)s")
                }
                Toggle("DEV9 Network", isOn: $settings.dev9Enabled)
                settingDescription("RetroAchievements requires a valid login before tracking games. DEV9 enables the PS2 network device path for supported titles.")
            }

            Section("Covers") {
                Button("Download Missing Covers") {
                    Task { await gameLibrary.downloadMissingCovers(template: settings.coverTemplate) }
                }
                HStack {
                    TextField("Advanced Cover Source", text: $settings.coverTemplate)
                        .onSubmit { settings.saveCoverTemplate() }
                    Button("Save") {
                        settings.saveCoverTemplate()
                    }
                }
                settingDescription("Cover templates support serial/title-style downloads. ARMSX2 also scans local cover folders and game metadata where available.")
            }

            Section("Emulator Backend") {
                HStack {
                    Text(launcher.executable?.path ?? "Auto-detect from the macOS branch build output")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .lineLimit(2)
                    Spacer()
                    Button("Choose") {
                        launcher.chooseExecutable()
                    }
                }
                settingDescription("The bundled backend should be used by default. Choose another executable only for development or testing a separate build.")
            }
        }
        .formStyle(.grouped)
        .navigationTitle("Settings")
        .padding(.top, 8)
    }

    private func settingDescription(_ text: String) -> some View {
        Text(text)
            .font(.caption)
            .foregroundStyle(.secondary)
            .fixedSize(horizontal: false, vertical: true)
    }
}

struct StorageView: View {
    @EnvironmentObject private var gameLibrary: GameLibrary

    var body: some View {
        Form {
            Section("Data Root") {
                HStack {
                    Text(MacPaths.dataRoot.path)
                        .font(.caption)
                        .textSelection(.enabled)
                    Spacer()
                    Button("Reveal") {
                        NSWorkspace.shared.activateFileViewerSelecting([MacPaths.dataRoot])
                    }
                }
            }

            Section("External Game Folders") {
                ForEach(gameLibrary.externalGameFolders(), id: \.path) { folder in
                    HStack {
                        Image(systemName: "folder")
                        Text(folder.path)
                            .textSelection(.enabled)
                        Spacer()
                        Button("Remove") {
                            gameLibrary.removeExternalFolder(folder.path)
                        }
                    }
                }

                Button {
                    if let folder = FilePanels.chooseFolder(title: "Add External Game Folder") {
                        gameLibrary.addExternalFolder(folder)
                    }
                } label: {
                    Label("Add Folder", systemImage: "plus")
                }
            }
        }
        .formStyle(.grouped)
        .navigationTitle("Storage")
    }
}

struct LogsView: View {
    private var logs: [URL] {
        (try? FileManager.default.contentsOfDirectory(at: MacPaths.directory("logs"), includingPropertiesForKeys: nil, options: [.skipsHiddenFiles]))?
            .filter { ["txt", "log"].contains($0.pathExtension.lowercased()) }
            .sorted { $0.lastPathComponent > $1.lastPathComponent } ?? []
    }

    var body: some View {
        List(logs, id: \.path) { log in
            HStack {
                Image(systemName: "doc.text")
                VStack(alignment: .leading) {
                    Text(log.lastPathComponent)
                    Text(log.path)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                Spacer()
                Button("Open") {
                    NSWorkspace.shared.open(log)
                }
                Button("Reveal") {
                    NSWorkspace.shared.activateFileViewerSelecting([log])
                }
            }
            .padding(.vertical, 4)
        }
        .navigationTitle("Logs")
    }
}
