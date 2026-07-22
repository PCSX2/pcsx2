// GameListView.swift — ROM list with favorites
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import PhotosUI
import UniformTypeIdentifiers
import UIKit

/// Region-string to flag emoji mapping, shared by library cards and the game
/// info detail view. Returns an empty string for unknown regions so callers can
/// omit the flag without rendering a broken placeholder.
enum RegionFlag {
    static func emoji(for region: String?) -> String {
        guard let region, !region.isEmpty else { return "" }
        let value = region.lowercased()
        if value.contains("japan") || value.contains("ntsc-j") {
            return "🇯🇵"
        }
        if value.contains("usa") || value.contains("america") || value.contains("ntsc-u") {
            return "🇺🇸"
        }
        if value.contains("europe") || value.contains("pal") {
            return "🇪🇺"
        }
        if value.contains("korea") || value.contains("ntsc-k") {
            return "🇰🇷"
        }
        if value.contains("china") || value.contains("ntsc-c") {
            return "🇨🇳"
        }
        if value.contains("hong kong") || value.contains("ntsc-hk") {
            return "🇭🇰"
        }
        if value.contains("australia") {
            return "🇦🇺"
        }
        return ""
    }
}

struct ISOEntry: Identifiable {
	var id: String { bootPath ?? fileURL?.path ?? name }
	let name: String
	let fileURL: URL?
	let bootPath: String?
	let coverURL: URL?
	let coverSignature: String?
	let metadata: [String: String]
	let size: UInt64
	var isFavorite: Bool
	var isExternal: Bool = false
	var sourceName: String? = nil

	var bootName: String {
		isExternal ? (bootPath ?? fileURL?.path ?? name) : name
	}

	var isELF: Bool {
		(bootPath ?? fileURL?.path ?? name).lowercased().hasSuffix(".elf")
	}

	var coverInfo: CoverGameInfo {
		CoverGameInfo(name: name, fileURL: fileURL, metadata: metadata, hasCover: coverURL != nil)
	}
}

@MainActor
private final class GameLibrarySnapshot {
	struct CachedGameMetadata: Codable {
		let metadata: [String: String]
		let modificationDate: Date?
		let size: UInt64
	}

	static let shared = GameLibrarySnapshot()

	private var entriesByID: [String: ISOEntry] = [:]
	private var orderedEntries: [ISOEntry] = []
	// Per-game metadata keyed by entry id and validated against the ISO file's
	// modification time (falling back to file size), so reloading the library reuses
	// known serial/CRC/region instead of opening every ISO again. Persisted to disk
	// so a cold app launch does not rescan the whole library.
	private var metadataCache: [String: CachedGameMetadata] = [:]
	private var metadataCacheDirty = false

	private static var persistenceURL: URL? {
		let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first
			?? FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first
		return base?.appendingPathComponent("LibraryMetadataCache.json")
	}

	private init() {
		guard let url = Self.persistenceURL,
		      let data = try? Data(contentsOf: url) else { return }
		if let decoded = try? JSONDecoder().decode([String: CachedGameMetadata].self, from: data) {
			metadataCache = decoded
		}
	}

	var entries: [ISOEntry] {
		orderedEntries
	}

	func existingEntries(merging currentEntries: [ISOEntry]) -> [String: ISOEntry] {
		var merged = entriesByID
		for entry in currentEntries {
			merged[entry.id] = entry
		}
		return merged
	}

	func update(_ entries: [ISOEntry]) {
		orderedEntries = entries
		entriesByID = Dictionary(entries.map { ($0.id, $0) }, uniquingKeysWith: { current, _ in current })
	}

	func releaseEntriesForGameplay() {
		orderedEntries.removeAll(keepingCapacity: false)
		entriesByID.removeAll(keepingCapacity: false)
	}

	/// Returns cached metadata when the file is unchanged: a matching modification
	/// time, or a matching size when the modification time is unavailable (some
	/// external folders report no stable date).
	func cachedMetadata(for id: String, modificationDate: Date?, size: UInt64) -> [String: String]? {
		guard let cached = metadataCache[id] else { return nil }
		if let fileDate = modificationDate, let cachedDate = cached.modificationDate, fileDate == cachedDate {
			return cached.metadata
		}
		if modificationDate == nil, cached.size > 0, cached.size == size {
			return cached.metadata
		}
		return nil
	}

	func storeMetadata(_ metadata: [String: String], modificationDate: Date?, size: UInt64, for id: String) {
		metadataCache[id] = CachedGameMetadata(metadata: metadata, modificationDate: modificationDate, size: size)
		metadataCacheDirty = true
	}

	/// Drops metadata for games no longer in the library so deleted files do not linger.
	func purgeMetadata(keeping ids: Set<String>) {
		let before = metadataCache.count
		metadataCache = metadataCache.filter { ids.contains($0.key) }
		if metadataCache.count != before {
			metadataCacheDirty = true
		}
	}

	/// Writes the metadata cache to disk if it changed since the last save.
	func persistMetadataCache() {
		guard metadataCacheDirty else { return }
		metadataCacheDirty = false
		guard let url = Self.persistenceURL else { return }
		try? FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
		guard let data = try? JSONEncoder().encode(metadataCache) else { return }
		try? data.write(to: url, options: .atomic)
	}
}

struct GameListView: View {
    @State private var games: [ISOEntry] = []
    @State private var appState = AppState.shared
	@State private var settings = SettingsStore.shared
	@State private var fileImporter = FileImportHandler.shared
	@State private var coverStore = CoverStore.shared
	@State private var externalLibrary = ExternalGameLibrary.shared
	@State private var externalCoverAutoDownloadAttemptedIDs = Set<String>()
	@State private var coverWorkTask: Task<Void, Never>?
	@State private var showGameImporter = false
	@State private var isLoadingGames = false
	@State private var showCoverImporter = false
    @State private var showCoverPhotoPicker = false
    @Environment(\.menuTabIsActive) private var menuTabIsActive
    @State private var showRestartAlert = false
    @State private var showStopAlert = false
    @State private var showCoverTemplateEditor = false
    @State private var showGameReplacementAlert = false
    @State private var coverTemplateDraft = CoverStore.defaultCoverURLTemplate
    @State private var pendingGameName: String = ""
    @State private var pendingGameImportURLs: [URL] = []
    @State private var existingGameImportFileNames: [String] = []
    @State private var pendingCoverGameName: String?
    @State private var pendingCoverPhotoGameName: String?
    @State private var selectedCoverPhotoItem: PhotosPickerItem?
    @State private var gameInfoTarget: ISOEntry?
    @State private var gameSettingsTarget: ISOEntry?
    @State private var discLinkTarget: ISOEntry?
    @State private var cheatsManagerTarget: ISOEntry?
    @State private var pendingDeleteGame: ISOEntry?
    @State private var pendingDeleteDataGame: ISOEntry?
    @State private var gameActionTitle = ""
    @State private var gameActionMessage: String?
    @State private var showBackgroundAssetError = false
    @AppStorage("ARMSX2iOSGameLibraryLayout") private var libraryLayout = "grid"
    @AppStorage("ARMSX2iOSLandscapeCoverFlowEnabled") private var landscapeCoverFlowEnabled = true
#if DEBUG
    @State private var ranBackgroundValidation = false
#endif
    // Background state is now kept in SettingsStore and rendered by BackgroundContainerView.
    private var hasCustomBackground: Bool {
        settings.dynamicBackgroundsEnabled
            || settings.backgroundPrimaryAsset != nil
            || settings.backgroundLandscapeAsset != nil
    }

    private var shouldRenderLibraryBackground: Bool {
		menuTabIsActive
    }

    private struct CoverFlowMetrics {
        let isCompact: Bool
        let coverWidth: CGFloat
        let coverHeight: CGFloat
        let textWidth: CGFloat
        let cardSpacing: CGFloat
        let cardPadding: CGFloat
        let cornerRadius: CGFloat
        let favoritePadding: CGFloat
        let favoriteInset: CGFloat
        let itemSpacing: CGFloat
        let horizontalPadding: CGFloat
        let verticalPadding: CGFloat
        let statusWidth: CGFloat
        let statusHeight: CGFloat
        let statusIconSize: CGFloat

        init(containerSize: CGSize) {
            isCompact = containerSize.height < 360
            coverWidth = isCompact ? 104 : 150
            coverHeight = isCompact ? 156 : 225
            textWidth = isCompact ? 134 : 164
            cardSpacing = isCompact ? 8 : 12
            cardPadding = isCompact ? 8 : 12
            cornerRadius = isCompact ? 18 : 24
            favoritePadding = isCompact ? 6 : 8
            favoriteInset = isCompact ? 5 : 8
            itemSpacing = isCompact ? 14 : 20
            horizontalPadding = isCompact ? 20 : 32
            verticalPadding = isCompact ? 10 : 18
            statusWidth = isCompact ? 138 : 166
            statusHeight = isCompact ? 190 : 276
            statusIconSize = isCompact ? 36 : 48
        }
    }

    var body: some View {
        NavigationStack {
            ZStack {
                if hasCustomBackground && shouldRenderLibraryBackground {
                    MenuBackgroundLayer()
                }

                GeometryReader { geo in
                    Group {
                        if games.isEmpty && appState.runningGameName == nil {
                            emptyState
                        } else if libraryLayout == "grid" && geo.size.width > geo.size.height && landscapeCoverFlowEnabled {
                            coverFlowLibrary(containerSize: geo.size)
                        } else if libraryLayout == "grid" {
                            gridLibrary
                        } else {
                            listLibrary
#if targetEnvironment(macCatalyst)
                            .listStyle(.inset)
#endif
                        }
                    }
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                }
            }
			.navigationTitle(settings.localized("Games"))
			.toolbarBackground(hasCustomBackground ? .hidden : .automatic, for: .navigationBar)
				.toolbar {
					ToolbarItem(placement: .topBarTrailing) {
						Button {
							showGameImporter = true
						} label: {
							Image(systemName: "plus")
						}
						.accessibilityLabel(settings.localized("Import Games"))
					}
                ToolbarItem(placement: .topBarTrailing) {
                    Menu {
                        Button {
                            libraryLayout = libraryLayout == "grid" ? "list" : "grid"
                        } label: {
                            Label(
                                settings.localized(libraryLayout == "grid" ? "Show List" : "Show Grid"),
                                systemImage: libraryLayout == "grid" ? "list.bullet" : "square.grid.2x2"
                            )
                        }

                        if libraryLayout == "grid" {
                            Toggle(isOn: $landscapeCoverFlowEnabled) {
                                Label(settings.localized("Landscape Cover Flow"), systemImage: "rectangle.landscape.rotate")
                            }
                        }
                    } label: {
                        Image(systemName: libraryLayout == "grid" ? "list.bullet" : "square.grid.2x2")
                    }
                    .accessibilityLabel(settings.localized("Library Layout"))
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Menu {
                        Button {
                            presentMenuPanel("cover_import_all") {
                                pendingCoverGameName = nil
                                showCoverImporter = true
                            }
                        } label: {
                            Label(settings.localized("Import Local Covers"), systemImage: "photo.badge.plus")
                        }

                        Button {
                            downloadMissingCovers()
                        } label: {
                            Label(settings.localized("Download Missing Covers"), systemImage: "icloud.and.arrow.down")
                        }
                        .disabled(coverStore.isDownloadingCovers || games.isEmpty)

                        Button {
                            presentMenuPanel("cover_source") {
                                coverTemplateDraft = coverStore.coverURLTemplate
                                showCoverTemplateEditor = true
                            }
                        } label: {
                            Label(settings.localized("Cover Source"), systemImage: "link")
                        }

                        Button {
                            presentMenuPanel("cover_template_reset") {
                                coverStore.coverURLTemplate = CoverStore.defaultCoverURLTemplate
                                coverStore.lastCoverMessage = "Cover URL template reset to the ARMSX2 Android default."
                                coverStore.showCoverAlert = true
                            }
                        } label: {
                            Label(settings.localized("Reset Cover Template"), systemImage: "arrow.counterclockwise")
                        }
                    } label: {
                        Image(systemName: coverStore.isDownloadingCovers ? "icloud.and.arrow.down" : "photo.stack")
                    }
                    .accessibilityLabel(settings.localized("Covers"))
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button { loadGames(forceMetadataRefresh: true) } label: {
                        Image(systemName: "arrow.clockwise")
                    }
                    .disabled(isLoadingGames)
                    .accessibilityLabel(settings.localized("Refresh"))
                }
                ToolbarItem(placement: .topBarLeading) {
                    if ARMSX2Bridge.hasBIOS() {
                        Button(settings.localized("BIOS Only")) {
                            if appState.runningGameName == "BIOS" {
                                appState.returnToGame()
                            } else if appState.runningGameName != nil {
                                pendingGameName = ""
                                showRestartAlert = true
                            } else {
                                appState.bootBIOSOnly()
                            }
                        }
                        .font(.callout)
                    }
                }
            }
            .alert(settings.localized("Cover Result"), isPresented: $coverStore.showCoverAlert) {
                Button(settings.localized("OK")) {}
            } message: {
                Text(coverStore.lastCoverMessage ?? "")
            }
            .alert(settings.localized("Cover Source"), isPresented: $showCoverTemplateEditor) {
                TextField("https://.../${serial}.jpg", text: $coverTemplateDraft)
                    .textInputAutocapitalization(.never)
                    .autocorrectionDisabled()
                Button(settings.localized("Cancel"), role: .cancel) {}
                Button(settings.localized("Save")) {
                    coverStore.coverURLTemplate = coverTemplateDraft
                    if games.isEmpty {
                        coverStore.lastCoverMessage = "Cover URL template saved."
                        coverStore.showCoverAlert = true
                    } else {
                        downloadMissingCovers()
                    }
                }
            } message: {
                Text("Use ${serial}, ${title}, or ${filetitle}. Default: \(CoverStore.defaultCoverURLTemplate)")
            }
            .alert(settings.localized("Restart VM?"), isPresented: $showRestartAlert) {
                Button(settings.localized("Cancel"), role: .cancel) {}
                Button(settings.localized("Restart"), role: .destructive) {
                    if pendingGameName.isEmpty {
                        appState.shutdownAndBootBIOS()
                    } else {
                        appState.shutdownAndBoot(isoName: pendingGameName)
                    }
                }
			} message: {
				let target = pendingGameName.isEmpty ? "BIOS Only" : (pendingGameName as NSString).lastPathComponent
				Text("\(settings.localized("VM is currently running."))\n\(settings.localized("Shut down and start")) \(settings.localized(target))?")
			}
            .alert(
                settings.localized("Delete Game Data?"),
                isPresented: Binding(
                    get: { pendingDeleteDataGame != nil },
                    set: { if !$0 { pendingDeleteDataGame = nil } }
                )
            ) {
                Button(settings.localized("Cancel"), role: .cancel) {
                    pendingDeleteDataGame = nil
                }
                Button(settings.localized("Delete Game Data"), role: .destructive) {
                    if let game = pendingDeleteDataGame {
                        deleteGameData(game)
                    }
                    pendingDeleteDataGame = nil
                }
            } message: {
                Text(settings.localized("This clears save states, PNACH files, per-game settings, compatibility overrides, and generated cache for this game. Memory card contents are not deleted."))
            }
            .alert(
                settings.localized("Delete Game?"),
                isPresented: Binding(
                    get: { pendingDeleteGame != nil },
                    set: { if !$0 { pendingDeleteGame = nil } }
                )
            ) {
                Button(settings.localized("Cancel"), role: .cancel) {
                    pendingDeleteGame = nil
                }
                Button(settings.localized("Delete ROM"), role: .destructive) {
                    if let game = pendingDeleteGame {
                        deleteGame(game, deleteData: false)
                    }
                    pendingDeleteGame = nil
                }
                Button(settings.localized("Delete ROM + Game Data"), role: .destructive) {
                    if let game = pendingDeleteGame {
                        deleteGame(game, deleteData: true)
                    }
                    pendingDeleteGame = nil
                }
            } message: {
                Text(settings.localized("Delete the selected game file? You can also remove its generated game data at the same time."))
            }
            .alert(
                settings.localized(gameActionTitle.isEmpty ? "Game Action" : gameActionTitle),
                isPresented: Binding(
                    get: { gameActionMessage != nil },
                    set: { if !$0 { gameActionMessage = nil } }
                )
            ) {
                Button(settings.localized("OK")) {
                    gameActionMessage = nil
                }
            } message: {
                Text(gameActionMessage ?? "")
            }
            .alert(settings.localized("Replace existing files?"), isPresented: $showGameReplacementAlert) {
                Button(settings.localized("Cancel"), role: .cancel) {
                    clearPendingGameImport()
                }
                Button(settings.localized("Replace"), role: .destructive) {
                    importGames(pendingGameImportURLs, allowReplacingExistingFiles: true)
                    clearPendingGameImport()
                }
            } message: {
                Text(FileImportHandler.replacementConfirmationMessage(for: existingGameImportFileNames))
            }
            .alert(settings.localized("Background image could not be loaded."), isPresented: $showBackgroundAssetError) {
                Button(settings.localized("OK")) {
                    showBackgroundAssetError = false
                }
            }
				.sheet(isPresented: $showGameImporter) {
					ImportDocumentPicker(
						allowedContentTypes: FileImportHandler.gameContentTypes,
						allowsMultipleSelection: true,
						legacyDocumentTypes: ["public.item", "public.data", "public.content"]
	                ) { result in
                    showGameImporter = false
                    switch result {
                    case .success(let urls):
                        prepareGameImport(urls)
                    case .failure(let error):
                        if !FileImportHandler.isUserCancelledPickerError(error) {
                            fileImporter.presentImportResult(FileImportHandler.failedGamePickerMessage(errorDescription: error.localizedDescription))
                        }
					}
				}
			}
			.sheet(isPresented: $showCoverImporter) {
				ImportDocumentPicker(
					allowedContentTypes: CoverStore.coverContentTypes,
                    allowsMultipleSelection: pendingCoverGameName == nil
                ) { result in
                    showCoverImporter = false
                    switch result {
                    case .success(let urls):
                        coverStore.importCoverURLs(urls, forGameNamed: pendingCoverGameName)
                        pendingCoverGameName = nil
                        loadGames()
                    case .failure(let error):
                        if !FileImportHandler.isUserCancelledPickerError(error) {
                            coverStore.lastCoverMessage = "Cover import failed: \(error.localizedDescription)"
                            coverStore.showCoverAlert = true
                        }
                        pendingCoverGameName = nil
                    }
                }
            }
            .photosPicker(
                isPresented: $showCoverPhotoPicker,
                selection: $selectedCoverPhotoItem,
                matching: .images
            )
            .onChange(of: selectedCoverPhotoItem) { _, photoItem in
                guard let photoItem, let gameName = pendingCoverPhotoGameName else { return }
                selectedCoverPhotoItem = nil
                pendingCoverPhotoGameName = nil
                importCoverPhoto(photoItem, forGameNamed: gameName)
            }
            .sheet(item: $gameInfoTarget) { game in
                GameInfoPanel(game: game, coverStore: coverStore)
                    .presentationDetents([.medium, .large])
            }
            .sheet(item: $gameSettingsTarget) { game in
                PerGameSettingsPanel(game: game)
                    .presentationDetents([.large])
                    .presentationDragIndicator(.visible)
                    .presentationBackground(.clear)
                    .presentationCornerRadius(34)
            }
            .sheet(item: $discLinkTarget) { game in
                DiscLinkPicker(discs: games.filter { !$0.isELF && $0.id != game.id }) { selected in
                    ARMSX2Bridge.setLinkedDiscPath(selected?.fileURL?.path ?? selected?.bootName, forELF: game.bootName)
                    loadGames()
                }
                .presentationDetents([.medium, .large])
            }
            .sheet(item: $cheatsManagerTarget) { game in
                CheatsPatchesManagerView(
                    isoName: game.bootName,
                    gameTitle: game.name,
                    launchContext: .library
                )
                    .presentationDetents([.large])
                    .presentationDragIndicator(.visible)
            }
		}
		.onAppear {
			CoverThumbnailCache.shared.activateForMenu()
			externalLibrary.reload()
			restoreCachedGamesIfNeeded()
			loadGames(autoDownloadExternalCovers: true)
			if settings.sanitizeBackgroundAssets() {
				showBackgroundAssetError = true
			}
#if DEBUG
			if !ranBackgroundValidation {
				ranBackgroundValidation = true
				BackgroundValidation.run()
			}
#endif
		}
		.onReceive(NotificationCenter.default.publisher(for: ExternalGameLibrary.didChangeNotification)) { _ in
			loadGames(autoDownloadExternalCovers: true)
		}
		.onReceive(NotificationCenter.default.publisher(for: NSNotification.Name("ARMSX2iOSReturnToMenu"))) { _ in
			restoreCachedGamesIfNeeded()
			loadGames(autoDownloadExternalCovers: false)
		}
		.onDisappear {
			guard case .playing = appState.currentScreen else { return }
			releaseLibraryResourcesForGameplay()
		}
    }

    private var listLibrary: some View {
        List {
            if let gameName = appState.runningGameName {
                vmStatusSection(gameName: gameName)
            }
            ForEach(games) { game in
                gameRow(game)
                    .menuBackgroundListRow(hasCustomBackground)
            }
        }
        .scrollContentBackground(hasCustomBackground ? .hidden : .automatic)
    }

    private var gridLibrary: some View {
        ScrollView {
            LazyVStack(spacing: 16) {
                if let gameName = appState.runningGameName {
                    vmStatusCard(gameName: gameName)
                        .padding(.horizontal)
                }

                LazyVGrid(columns: [GridItem(.adaptive(minimum: 142), spacing: 14, alignment: .top)], spacing: 18) {
                    ForEach(games) { game in
                        gameGridCard(game)
                    }
                }
                .padding(.horizontal)
                .padding(.bottom, 20)
            }
            .padding(.top, 12)
        }
        .background(
            Group {
                if hasCustomBackground {
                    Color.clear
                } else {
                    LinearGradient(
                        colors: [Color(.systemGroupedBackground), Color(.secondarySystemGroupedBackground)],
                        startPoint: .top,
                        endPoint: .bottom
                    )
                }
            }
        )
        .transaction { transaction in
            transaction.animation = nil
        }
    }

    private func coverFlowLibrary(containerSize: CGSize) -> some View {
        let metrics = CoverFlowMetrics(containerSize: containerSize)

        return ScrollView(.horizontal, showsIndicators: false) {
            LazyHStack(alignment: .center, spacing: metrics.itemSpacing) {
                if let gameName = appState.runningGameName {
                    vmStatusCoverCard(gameName: gameName, metrics: metrics)
                }

                ForEach(games) { game in
                    coverFlowCard(game, metrics: metrics)
                }
            }
            .frame(
                minWidth: max(0, containerSize.width - (metrics.horizontalPadding * 2)),
                minHeight: max(0, containerSize.height - (metrics.verticalPadding * 2)),
                alignment: .center
            )
            .padding(.horizontal, metrics.horizontalPadding)
            .padding(.vertical, metrics.verticalPadding)
        }
        .background(
            Group {
                if hasCustomBackground {
                    Color.clear
                } else {
                    LinearGradient(
                        colors: [Color(.systemGroupedBackground), Color(.secondarySystemGroupedBackground)],
                        startPoint: .top,
                        endPoint: .bottom
                    )
                }
            }
        )
        .transaction { transaction in
            transaction.animation = nil
        }
    }

    /// Clean display title for the running game, preferring the library entry over the raw path.
    private func displayTitle(forRunningName name: String) -> String {
        if name == "BIOS" {
            return settings.localized("BIOS Only")
        }
        if let entry = games.first(where: { $0.bootName == name || $0.name == name }) {
            let title = entry.metadata["title"]?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
            return title.isEmpty ? entry.name : title
        }
        return Self.cleanGameFileName(name)
    }

    /// Last path component with its disc extension (`.iso`, `.bin`, `.cue`, …) removed,
    /// used when no matching library entry is available.
    private static func cleanGameFileName(_ value: String) -> String {
        let fileName = (value as NSString).lastPathComponent
        guard !fileName.isEmpty else { return value }
        return (fileName as NSString).deletingPathExtension
    }

    private func vmStatusSection(gameName: String) -> some View {
        Section {
            // Resume row — tap anywhere to return to game
            Button {
                appState.returnToGame()
            } label: {
                HStack {
                    Image(systemName: "play.circle.fill")
                        .foregroundStyle(.green)
                        .font(.title)
                    VStack(alignment: .leading, spacing: 2) {
                        Text(settings.localized("Now Running"))
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        Text(displayTitle(forRunningName: gameName))
                            .font(.body)
                            .fontWeight(.semibold)
                            .lineLimit(1)
                            .truncationMode(.tail)
                    }
                    Spacer()
                    Text(settings.localized("Resume"))
                        .font(.subheadline)
                        .fontWeight(.medium)
                    Image(systemName: "chevron.right")
                        .font(.caption)
                        .foregroundStyle(.tertiary)
                }
                .padding(.vertical, 6)
            }
            .tint(.primary)
            .menuBackgroundListRow(hasCustomBackground)

            // Stop button — separate row with confirmation alert
            Button(role: .destructive) {
                showStopAlert = true
            } label: {
                HStack {
                    Spacer()
                    Label(settings.localized("Stop Emulation"), systemImage: "stop.circle")
                        .font(.subheadline)
                    Spacer()
                }
            }
            .menuBackgroundListRow(hasCustomBackground)
        }
        .alert(settings.localized("Stop Emulation?"), isPresented: $showStopAlert) {
            Button(settings.localized("Cancel"), role: .cancel) { }
            Button(settings.localized("Stop"), role: .destructive) {
                ARMSX2Bridge.requestVMStop()
                appState.runningGameName = nil
            }
        } message: {
            Text(settings.localized("This will shut down the running game. All unsaved progress will be lost."))
        }
    }

    private func gameRow(_ game: ISOEntry) -> some View {
		let running = isRunning(game)
		return Button {
            open(game)
        } label: {
            HStack(spacing: 12) {
				coverThumbnail(for: game, running: running)

                VStack(alignment: .leading, spacing: 4) {
                    HStack(spacing: 6) {
                        Text(coverStore.displayName(forGameName: game.name))
                            .font(.body)
                            .fontWeight(.medium)
                            .foregroundStyle(.primary)
						if running {
							Image(systemName: "circle.fill")
								.font(.system(size: 8))
								.foregroundStyle(.green)
								.accessibilityLabel(settings.localized("Running"))
						}
                    }
                    HStack(spacing: 8) {
						Text(formatSize(game.size))
						Text(game.name.pathExtensionLabel)
						if game.isExternal {
							Label(settings.localized("External"), systemImage: "externaldrive")
						}
						if let flag = regionFlagText(for: game) {
							Text(flag).accessibilityLabel(settings.localized("Region"))
						}
					}
					.font(.caption)
					.foregroundStyle(.secondary)
				}
				Spacer()
				Button {
					toggleFavorite(game)
				} label: {
					Image(systemName: game.isFavorite ? "star.fill" : "star")
						.foregroundStyle(game.isFavorite ? .yellow : .gray)
						.frame(width: 44, height: 44)
						.contentShape(Rectangle())
				}
				.buttonStyle(.plain)
				.accessibilityLabel(game.isFavorite ? settings.localized("Remove from favorites") : settings.localized("Add to favorites"))

				Image(systemName: running ? "play.fill" : "chevron.right")
					.foregroundStyle(running ? .green : .secondary)
					.font(.caption)
					.accessibilityHidden(true)
			}
		}
        .foregroundStyle(.primary)
        .contextMenu {
            gameContextMenu(for: game)
        }
    }

    private func gameGridCard(_ game: ISOEntry) -> some View {
		let running = isRunning(game)
		return Button {
			open(game)
		} label: {
            VStack(alignment: .center, spacing: 10) {
				ZStack(alignment: .topTrailing) {
					coverThumbnail(for: game, width: 126, height: 189, running: running)
						.frame(maxWidth: .infinity)

					Button {
						toggleFavorite(game)
					} label: {
						Image(systemName: game.isFavorite ? "star.fill" : "star")
							.font(.callout.weight(.semibold))
							.foregroundStyle(game.isFavorite ? .yellow : .white.opacity(0.86))
							.padding(6)
							.background(.black.opacity(0.36), in: Circle())
					}
					.buttonStyle(.plain)
					.padding(6)
					.accessibilityLabel(game.isFavorite ? settings.localized("Remove from favorites") : settings.localized("Add to favorites"))
				}

                VStack(alignment: .center, spacing: 4) {
                    HStack(alignment: .firstTextBaseline, spacing: 5) {
                        Text(coverStore.displayName(forGameName: game.name))
                            .font(.subheadline.weight(.semibold))
                            .foregroundStyle(.primary)
                            .lineLimit(2)
                            .multilineTextAlignment(.center)
                            .frame(maxWidth: .infinity, alignment: .center)
						if running {
                            Image(systemName: "circle.fill")
                                .font(.system(size: 7))
                                .foregroundStyle(.green)
                                .accessibilityLabel(settings.localized("Running"))
                        }
                    }
                    // Reserve space for two title lines so cards stay aligned
                    // whether a title wraps or fits on one line. Uses a minimum
                    // height so accessibility Dynamic Type sizes can grow beyond
                    // it instead of being clipped.
                    .frame(minHeight: 38, alignment: .top)
                    HStack(spacing: 6) {
                        Text(game.name.pathExtensionLabel)
                        Text(formatSize(game.size))
                        if game.isExternal {
                            Image(systemName: "externaldrive")
                                .accessibilityLabel(settings.localized("External"))
                        }
                        if let flag = regionFlagText(for: game), !flag.isEmpty {
                            Text(flag).accessibilityLabel(settings.localized("Region"))
                        }
                    }
                    .font(.caption2)
                    .foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity, alignment: .center)
                }
                .frame(maxWidth: .infinity, alignment: .center)
            }
            .padding(12)
            .frame(maxWidth: .infinity, minHeight: 268, alignment: .top)
			.glassSurface(clear: true, cornerRadius: 18)
        }
		.buttonStyle(.plain)
		.contextMenu {
			gameContextMenu(for: game)
		}
    }

    private func coverFlowCard(_ game: ISOEntry, metrics: CoverFlowMetrics) -> some View {
		let running = isRunning(game)
		return Button {
			open(game)
		} label: {
            VStack(spacing: metrics.cardSpacing) {
				ZStack(alignment: .topTrailing) {
					coverThumbnail(
						for: game,
						width: metrics.coverWidth,
						height: metrics.coverHeight,
						running: running
					)
					.shadow(color: running ? .clear : .black.opacity(0.28), radius: 18, y: 10)

					Button {
						toggleFavorite(game)
					} label: {
						Image(systemName: game.isFavorite ? "star.fill" : "star")
							.font((metrics.isCompact ? Font.subheadline : Font.headline).weight(.semibold))
							.foregroundStyle(game.isFavorite ? .yellow : .white.opacity(0.88))
							.padding(metrics.favoritePadding)
							.background(.black.opacity(0.48), in: Circle())
					}
					.buttonStyle(.plain)
					.padding(metrics.favoriteInset)
					.accessibilityLabel(game.isFavorite ? settings.localized("Remove from favorites") : settings.localized("Add to favorites"))
				}

                VStack(spacing: 4) {
                    Text(coverStore.displayName(forGameName: game.name))
                        .font((metrics.isCompact ? Font.subheadline : Font.headline).weight(.semibold))
                        .multilineTextAlignment(.center)
                        .lineLimit(2)
                        // minHeight keeps cards aligned for 1- vs 2-line titles
                        // while letting Dynamic Type grow beyond it without clipping.
                        .frame(minHeight: metrics.isCompact ? 38 : 46, alignment: .top)
                    HStack(spacing: 4) {
                        Text(game.isExternal ? "\(game.name.pathExtensionLabel)  \(formatSize(game.size))  \(settings.localized("External"))" : "\(game.name.pathExtensionLabel)  \(formatSize(game.size))")
                        if let flag = regionFlagText(for: game) {
                            Text(flag).accessibilityLabel(settings.localized("Region"))
                        }
                    }
                    .font(metrics.isCompact ? .caption2 : .caption)
                    .foregroundStyle(.secondary)
                }
                .frame(width: metrics.textWidth)
            }
            .padding(metrics.cardPadding)
			.glassSurface(clear: true, cornerRadius: metrics.cornerRadius)
        }
		.buttonStyle(.plain)
		.contextMenu {
			gameContextMenu(for: game)
		}
    }

	private func coverThumbnail(
		for game: ISOEntry,
		width: CGFloat = 58,
		height: CGFloat = 87,
		running: Bool
	) -> some View {
		CoverThumbnailView(
            gameName: game.name,
            coverURL: game.coverURL,
            coverSignature: game.coverSignature,
            width: width,
            height: height
        )
		.shadow(
			color: running ? .green.opacity(0.7) : .clear,
			radius: running ? 12 : 0,
			y: running ? 4 : 0
		)
    }

    private func vmStatusCard(gameName: String) -> some View {
        HStack(spacing: 12) {
            Image(systemName: "play.circle.fill")
                .font(.title2)
                .foregroundStyle(.green)
            VStack(alignment: .leading, spacing: 3) {
                Text(settings.localized("Now Running"))
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Text(displayTitle(forRunningName: gameName))
                    .font(.subheadline.weight(.semibold))
                    .lineLimit(1)
                    .truncationMode(.tail)
            }
            Spacer()
            Button(settings.localized("Resume")) {
                appState.returnToGame()
            }
            .buttonStyle(.borderedProminent)
            Button(role: .destructive) {
                showStopAlert = true
            } label: {
                Image(systemName: "stop.circle")
            }
            .buttonStyle(.bordered)
        }
        .padding()
		.glassSurface(clear: true, cornerRadius: 18)
        .alert(settings.localized("Stop Emulation?"), isPresented: $showStopAlert) {
            Button(settings.localized("Cancel"), role: .cancel) { }
            Button(settings.localized("Stop"), role: .destructive) {
                ARMSX2Bridge.requestVMStop()
                appState.runningGameName = nil
            }
        } message: {
            Text(settings.localized("This will shut down the running game. All unsaved progress will be lost."))
        }
    }

    private func vmStatusCoverCard(gameName: String, metrics: CoverFlowMetrics) -> some View {
        VStack(spacing: metrics.isCompact ? 9 : 14) {
            Image(systemName: "play.circle.fill")
                .font(.system(size: metrics.statusIconSize))
                .foregroundStyle(.green)
            Text(settings.localized("Now Running"))
                .font(.caption.weight(.semibold))
                .foregroundStyle(.secondary)
            Text(displayTitle(forRunningName: gameName))
                .font((metrics.isCompact ? Font.subheadline : Font.headline).weight(.semibold))
                .lineLimit(2)
                .multilineTextAlignment(.center)
            HStack(spacing: 8) {
                Button(settings.localized("Resume")) {
                    appState.returnToGame()
                }
                .buttonStyle(.borderedProminent)
                Button(role: .destructive) {
                    showStopAlert = true
                } label: {
                    Image(systemName: "stop.circle")
                }
                .buttonStyle(.bordered)
            }
            .controlSize(metrics.isCompact ? .small : .regular)
        }
        .frame(width: metrics.statusWidth, height: metrics.statusHeight)
        .padding(metrics.cardPadding)
		.glassSurface(clear: true, cornerRadius: metrics.cornerRadius)
        .alert(settings.localized("Stop Emulation?"), isPresented: $showStopAlert) {
            Button(settings.localized("Cancel"), role: .cancel) { }
            Button(settings.localized("Stop"), role: .destructive) {
                ARMSX2Bridge.requestVMStop()
                appState.runningGameName = nil
            }
        } message: {
            Text(settings.localized("This will shut down the running game. All unsaved progress will be lost."))
        }
    }

	@ViewBuilder
	private func gameContextMenu(for game: ISOEntry) -> some View {
        Button {
            presentMenuPanel("game_info") {
                gameInfoTarget = game
            }
        } label: {
            Label(settings.localized("Game Info"), systemImage: "info.circle")
        }

        Button {
            presentMenuPanel("per_game_settings") {
                gameSettingsTarget = game
            }
        } label: {
            Label(settings.localized("Per-Game Settings"), systemImage: "slider.horizontal.3")
        }

        if game.isELF {
            discPathMenu(for: game)
        }

        Button {
            presentMenuPanel("cheats_patches") {
                cheatsManagerTarget = game
            }
        } label: {
            Label(settings.localized("Cheats & Patches"), systemImage: "rectangle.stack.badge.plus")
        }

        Menu {
            Button {
                downloadCover(for: game)
            } label: {
                Label(settings.localized("Download Cover"), systemImage: "icloud.and.arrow.down")
            }
            .disabled(coverStore.isDownloadingCovers)

            Button {
                presentMenuPanel("cover_photos") {
                    pendingCoverPhotoGameName = game.name
                    showCoverPhotoPicker = true
                }
            } label: {
                Label(settings.localized("Choose from Photos"), systemImage: "photo.on.rectangle")
            }

            Button {
                presentMenuPanel("cover_files") {
                    pendingCoverGameName = game.name
                    showCoverImporter = true
                }
            } label: {
                Label(settings.localized("Choose from Files"), systemImage: "folder")
            }

            if game.coverURL != nil {
                Button(role: .destructive) {
                    coverStore.removeManagedCovers(forGameNamed: game.name)
                    loadGames()
                } label: {
                    Label(settings.localized("Remove Cover"), systemImage: "trash")
                }
            }
        } label: {
            Label(settings.localized("Covers"), systemImage: "photo.stack")
        }

        Divider()

        Menu {
            Button {
                clearGameCache(game)
            } label: {
                Label(settings.localized("Clear Game Cache"), systemImage: "trash.slash")
            }

            Button(role: .destructive) {
                presentMenuPanel("delete_game_data") {
                    pendingDeleteDataGame = game
                }
            } label: {
                Label(settings.localized("Delete Game Data"), systemImage: "externaldrive.badge.xmark")
            }

			if !game.isExternal {
				Button(role: .destructive) {
					presentMenuPanel("delete_game") {
						pendingDeleteGame = game
					}
				} label: {
					Label(settings.localized("Delete Game"), systemImage: "trash")
				}
			}
		} label: {
			Label(settings.localized("Game Data"), systemImage: "externaldrive")
		}
	}

	@ViewBuilder
	private func discPathMenu(for game: ISOEntry) -> some View {
		let linkedDisc = ARMSX2Bridge.linkedDiscPath(forELF: game.bootName)
		Menu {
			Button {
				presentMenuPanel("disc_path") {
					discLinkTarget = game
				}
			} label: {
				Label(settings.localized(linkedDisc?.isEmpty == false ? "Change Disc Image" : "Link Disc Image"), systemImage: "link")
			}

			if let linkedDisc, !linkedDisc.isEmpty {
				Button(role: .destructive) {
					ARMSX2Bridge.setLinkedDiscPath(nil, forELF: game.bootName)
					loadGames()
				} label: {
					Label(settings.localized("Remove Disc Link"), systemImage: "xmark.circle")
				}
			}
		} label: {
			Label(settings.localized("Disc Path"), systemImage: "opticaldisc")
		}
	}

	private func presentMenuPanel(_ name: String, _ action: @escaping () -> Void) {
		NSLog("[ARMSX2 iOS GameListMenu] present \(name)")
		DispatchQueue.main.asyncAfter(deadline: .now() + 0.12) {
			action()
		}
	}

	private func open(_ game: ISOEntry) {
		if isRunning(game) {
			appState.returnToGame()
			return
		}

		guard ARMSX2Bridge.hasBIOS() else {
			gameActionTitle = settings.localized("BIOS Required")
			gameActionMessage = settings.localized("Import a valid PS2 BIOS before starting games.")
			return
		}

		guard ARMSX2Bridge.canResolveISO(game.bootName) else {
			gameActionTitle = settings.localized("Game Not Found")
			gameActionMessage = settings.localized("This game file is no longer available. Refresh the library or import it again.")
			loadGames()
			return
		}

		if appState.runningGameName != nil {
			pendingGameName = game.bootName
			showRestartAlert = true
		} else {
			appState.bootGame(isoName: game.bootName)
		}
	}

    private var emptyState: some View {
        VStack(spacing: 16) {
            Image(systemName: "opticaldisc")
                .font(.system(size: 48))
                .foregroundStyle(.secondary)
                .accessibilityHidden(true)
            Text(settings.localized("No Games Found"))
                .font(.title2)
                .fontWeight(.semibold)
            Text(settings.localized("Import PS2 disc images to add them here."))
                .font(.body)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
            Button {
                showGameImporter = true
            } label: {
                Label(settings.localized("Import Games"), systemImage: "plus")
            }
            .buttonStyle(.borderedProminent)
			Label(settings.localized("External Games are in Settings > Storage"), systemImage: "externaldrive")
				.font(.caption)
				.foregroundStyle(.secondary)
			Text("\(settings.localized("Supported Formats")): ISO, CHD, BIN, CSO, ZSO, GZ, ELF")
				.font(.caption)
				.foregroundStyle(.secondary)
				.multilineTextAlignment(.center)
        }
        .padding()
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

	private func loadGames(autoDownloadExternalCovers: Bool = false, forceMetadataRefresh: Bool = false) {
		guard !isLoadingGames else { return }
		isLoadingGames = true
		defer { isLoadingGames = false }

		let fm = FileManager.default
		let allowFullMetadata = appState.runningGameName == nil
		let existingGames = GameLibrarySnapshot.shared.existingEntries(merging: games)
		externalLibrary.reload()
		games = ARMSX2Bridge.availableISOEntries().compactMap { rawEntry -> ISOEntry? in
			guard let name = rawEntry["name"] as? String,
			      let path = rawEntry["path"] as? String else {
				return nil
			}
			let external = (rawEntry["external"] as? NSNumber)?.boolValue ?? (rawEntry["external"] as? Bool ?? false)
			let source = rawEntry["source"] as? String
			let bootName = external ? path : name
			let attrs = try? fm.attributesOfItem(atPath: path)
			let size = attrs?[.size] as? UInt64 ?? 0
			let modificationDate = attrs?[.modificationDate] as? Date
			let fav = ARMSX2Bridge.isFavorite(bootName)
			let fileURL = URL(fileURLWithPath: path)
			let entryID = external ? path : fileURL.path
			let metadata: [String: String]
			if forceMetadataRefresh && allowFullMetadata {
				metadata = ARMSX2Bridge.gameMetadata(forISO: bootName)
				GameLibrarySnapshot.shared.storeMetadata(metadata, modificationDate: modificationDate, size: size, for: entryID)
			} else if !allowFullMetadata {
				if let existing = existingGames[entryID] {
					metadata = existing.metadata
				} else if let cached = GameLibrarySnapshot.shared.cachedMetadata(
					for: entryID,
					modificationDate: modificationDate,
					size: size
				) {
					metadata = cached
				} else {
					metadata = ["fileTitle": (name as NSString).deletingPathExtension]
				}
			} else if let cached = GameLibrarySnapshot.shared.cachedMetadata(for: entryID, modificationDate: modificationDate, size: size) {
				metadata = cached
			} else {
				metadata = ARMSX2Bridge.gameMetadata(forISO: bootName)
				GameLibrarySnapshot.shared.storeMetadata(metadata, modificationDate: modificationDate, size: size, for: entryID)
			}
			let coverURL = coverStore.coverURL(forGameName: name, gamePath: fileURL, metadata: metadata)
			let existingCover = retainedCover(from: existingGames[entryID])
			let resolvedCoverURL = coverURL ?? existingCover?.url
			let coverSignature = CoverThumbnailCache.signature(for: resolvedCoverURL) ?? existingCover?.signature
			return ISOEntry(
				name: name,
				fileURL: fileURL,
				bootPath: external ? path : nil,
				coverURL: resolvedCoverURL,
				coverSignature: coverSignature,
				metadata: metadata,
				size: size,
				isFavorite: fav,
				isExternal: external,
				sourceName: source
			)
		}.sorted { a, b in
			if a.isFavorite != b.isFavorite { return a.isFavorite }
			return a.name.localizedCaseInsensitiveCompare(b.name) == .orderedAscending
		}

		if !allowFullMetadata {
			NSLog("[ARMSX2 iOS GameList] skipped full ISO metadata refresh while VM is active")
		}

		GameLibrarySnapshot.shared.purgeMetadata(keeping: Set(games.map { $0.id }))
		GameLibrarySnapshot.shared.update(games)
		GameLibrarySnapshot.shared.persistMetadataCache()

		if autoDownloadExternalCovers {
			autoDownloadExternalCoversIfNeeded()
		}
	}

	private func restoreCachedGamesIfNeeded() {
		guard games.isEmpty else { return }
		let cachedGames = GameLibrarySnapshot.shared.entries
		if !cachedGames.isEmpty {
			games = cachedGames
		}
	}

	private func retainedCover(from entry: ISOEntry?) -> (url: URL, signature: String?)? {
		guard let url = entry?.coverURL else { return nil }
		guard FileManager.default.fileExists(atPath: url.path) else { return nil }
		return (url, entry?.coverSignature)
	}

    private func downloadMissingCovers() {
        let targets = games.map(\.coverInfo)
        coverWorkTask?.cancel()
        coverWorkTask = Task { @MainActor in
            _ = await coverStore.downloadMissingCovers(for: targets)
			guard !Task.isCancelled else { return }
            loadGames()
        }
    }

    private func prepareGameImport(_ urls: [URL]) {
        let existingFileNames = fileImporter.existingFileNames(for: urls, preferredDestination: .game)
        guard !existingFileNames.isEmpty else {
            importGames(urls, allowReplacingExistingFiles: false)
            return
        }

        pendingGameImportURLs = urls
        existingGameImportFileNames = existingFileNames
        showGameReplacementAlert = true
    }

    private func importGames(_ urls: [URL], allowReplacingExistingFiles: Bool) {
        let importedGames = fileImporter.importURLs(
            urls,
            preferredDestination: .game,
            allowReplacingExistingFiles: allowReplacingExistingFiles
        )
        loadGames()
        autoDownloadCovers(for: importedGames)
    }

    private func clearPendingGameImport() {
        pendingGameImportURLs = []
        existingGameImportFileNames = []
    }

    private func downloadCover(for game: ISOEntry) {
        coverWorkTask?.cancel()
        coverWorkTask = Task { @MainActor in
            _ = await coverStore.downloadMissingCovers(for: [game.coverInfo])
			guard !Task.isCancelled else { return }
            loadGames()
        }
    }

    private func importCoverPhoto(_ photoItem: PhotosPickerItem, forGameNamed gameName: String) {
        Task { @MainActor in
            do {
                guard let data = try await photoItem.loadTransferable(type: Data.self) else {
                    coverStore.lastCoverMessage = "The selected photo could not be loaded."
                    coverStore.showCoverAlert = true
                    return
                }
                coverStore.importCoverData(data, forGameNamed: gameName)
                loadGames()
            } catch {
                coverStore.lastCoverMessage = "Cover import failed: \(error.localizedDescription)"
                coverStore.showCoverAlert = true
            }
        }
    }

	private func autoDownloadCovers(for importedGames: [FileImportHandler.ImportedGame]) {
		guard !importedGames.isEmpty else { return }

		let targets = importedGames.map { game in
			let metadata = ARMSX2Bridge.gameMetadata(forISO: game.name)
			let existingCover = coverStore.coverURL(forGameName: game.name, gamePath: game.fileURL, metadata: metadata)
			return CoverGameInfo(name: game.name, fileURL: game.fileURL, metadata: metadata, hasCover: existingCover != nil)
		}

		coverWorkTask?.cancel()
		coverWorkTask = Task { @MainActor in
			let summary = await coverStore.downloadMissingCovers(for: targets, showResult: false)
			guard !Task.isCancelled else { return }
			if summary.downloaded > 0 {
				loadGames()
			}
		}
	}

	private func autoDownloadExternalCoversIfNeeded() {
		guard appState.runningGameName == nil else { return }

		let targets = games.filter { game in
			game.isExternal &&
			game.coverURL == nil &&
			!externalCoverAutoDownloadAttemptedIDs.contains(game.id)
		}
		guard !targets.isEmpty else { return }

		for game in targets {
			externalCoverAutoDownloadAttemptedIDs.insert(game.id)
		}

		let coverTargets = targets.map(\.coverInfo)
		let serials = coverTargets.map { $0.metadata["serial"] ?? "" }.filter { !$0.isEmpty }.joined(separator: ",")
		NSLog("[ARMSX2 iOS Covers] auto-download external missing covers count=%d serials=%@", targets.count, serials)
		coverWorkTask?.cancel()
		coverWorkTask = Task { @MainActor in
			let summary = await coverStore.downloadMissingCovers(for: coverTargets, showResult: false)
			guard !Task.isCancelled else { return }
			if summary.downloaded > 0 {
				loadGames()
			}
		}
	}

	private func releaseLibraryResourcesForGameplay() {
		coverWorkTask?.cancel()
		coverWorkTask = nil
		games.removeAll(keepingCapacity: false)
		externalCoverAutoDownloadAttemptedIDs.removeAll(keepingCapacity: false)
		GameLibrarySnapshot.shared.releaseEntriesForGameplay()
			CoverThumbnailCache.shared.releaseForGameplay()
		}

	private func toggleFavorite(_ game: ISOEntry) {
		let key = game.bootName
		let current = ARMSX2Bridge.isFavorite(key)
		ARMSX2Bridge.setFavorite(key, favorite: !current)
		loadGames()
	}

	private func clearGameCache(_ game: ISOEntry) {
		gameActionTitle = "Clear Game Cache"
		gameActionMessage = ARMSX2Bridge.clearCache(forISO: game.bootName)
	}

	private func deleteGameData(_ game: ISOEntry) {
		gameActionTitle = "Delete Game Data"
		gameActionMessage = ARMSX2Bridge.deleteGameData(forISO: game.bootName)
	}

	private func deleteGame(_ game: ISOEntry, deleteData: Bool) {
		if isRunning(game) {
			gameActionTitle = "Delete Game"
			gameActionMessage = settings.localized("Stop this game before deleting it.")
			return
		}

		let success = ARMSX2Bridge.deleteISO(game.bootName, deleteGameData: deleteData)
		if success {
			coverStore.removeManagedCovers(forGameNamed: game.name)
			loadGames()
        }
		gameActionTitle = "Delete Game"
		gameActionMessage = success ? settings.localized("Game deleted.") : settings.localized("Could not delete this game file.")
	}

	private func isRunning(_ game: ISOEntry) -> Bool {
		guard let runningGameName = appState.runningGameName else {
			return false
		}

		return Self.gameIdentifiersMatch(runningGameName, game.bootName)
			|| Self.gameIdentifiersMatch(runningGameName, game.name)
			|| game.fileURL.map { Self.gameIdentifiersMatch(runningGameName, $0.path) } == true
	}

	private static func gameIdentifiersMatch(_ first: String, _ second: String) -> Bool {
		let normalizedFirst = (first.removingPercentEncoding ?? first)
			.replacingOccurrences(of: "\\", with: "/")
		let normalizedSecond = (second.removingPercentEncoding ?? second)
			.replacingOccurrences(of: "\\", with: "/")
		if normalizedFirst.caseInsensitiveCompare(normalizedSecond) == .orderedSame {
			return true
		}

		let firstFileName = (normalizedFirst as NSString).lastPathComponent
		let secondFileName = (normalizedSecond as NSString).lastPathComponent
		return !firstFileName.isEmpty
			&& firstFileName.caseInsensitiveCompare(secondFileName) == .orderedSame
	}

	/// Region flag emoji for a game's metadata, or nil when unknown so the
	/// caller can omit it instead of rendering a placeholder.
	private func regionFlagText(for game: ISOEntry) -> String? {
		let region = game.metadata["region"]?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
		let flag = RegionFlag.emoji(for: region)
		return flag.isEmpty ? nil : flag
	}

    private func formatSize(_ bytes: UInt64) -> String {
        let gb = Double(bytes) / 1_073_741_824
        if gb >= 1.0 {
            return String(format: "%.1f GB", gb)
        }
        let mb = Double(bytes) / 1_048_576
        return String(format: "%.0f MB", mb)
    }

}

private struct GameInfoPanel: View {
    @Environment(\.dismiss) private var dismiss
    @State private var settings = SettingsStore.shared

    let game: ISOEntry
    let coverStore: CoverStore

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    HStack(spacing: 14) {
                        CoverThumbnailView(
                            gameName: game.name,
                            coverURL: game.coverURL,
                            coverSignature: game.coverSignature,
                            width: 84,
                            height: 126
                        )

                        VStack(alignment: .leading, spacing: 6) {
                            Text(coverStore.displayName(forGameName: game.name))
                                .font(.headline)
                            Text(game.name)
                                .font(.caption)
                                .foregroundStyle(.secondary)
                                .textSelection(.enabled)
                        }
                    }
                    .padding(.vertical, 4)
                }

                Section(settings.localized("Disc")) {
                    LabeledContent(settings.localized("Region")) {
                        Text(regionDisplay)
                    }
                    LabeledContent(settings.localized("Serial")) {
                        Text(metadataValue("serial"))
                            .textSelection(.enabled)
                    }
                    LabeledContent(settings.localized("CRC")) {
                        Text(metadataValue("crc"))
                            .textSelection(.enabled)
                    }
                    LabeledContent(settings.localized("Format")) {
                        Text(game.name.pathExtensionLabel)
                    }
                    LabeledContent(settings.localized("Size")) {
                        Text(formatSize(game.size))
                    }
                }

                Section(settings.localized("File")) {
                    Text(game.fileURL?.path ?? settings.localized("File path unavailable"))
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .textSelection(.enabled)
                }
            }
            .navigationTitle(settings.localized("Game Info"))
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button(settings.localized("Done")) {
                        dismiss()
                    }
                }
            }
        }
    }

    private var regionDisplay: String {
        let region = metadataValue("region")
        let flag = RegionFlag.emoji(for: region)
        return flag.isEmpty ? region : "\(flag) \(region)"
    }

    private func metadataValue(_ key: String) -> String {
        let value = game.metadata[key]?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
        return value.isEmpty ? settings.localized("Unknown") : value
    }

    private func formatSize(_ bytes: UInt64) -> String {
        let gb = Double(bytes) / 1_073_741_824
        if gb >= 1.0 {
            return String(format: "%.1f GB", gb)
        }
        let mb = Double(bytes) / 1_048_576
        return String(format: "%.0f MB", mb)
    }
}

private struct DiscLinkPicker: View {
    @Environment(\.dismiss) private var dismiss
    @State private var settings = SettingsStore.shared

    let discs: [ISOEntry]
    let onSelect: (ISOEntry?) -> Void

    var body: some View {
        NavigationStack {
            List {
                if discs.isEmpty {
                    Text(settings.localized("No disc images found. Import an ISO first, then link it here."))
                        .foregroundStyle(.secondary)
                } else {
                    ForEach(discs) { disc in
                        Button {
                            onSelect(disc)
                            dismiss()
                        } label: {
                            Text(disc.name).lineLimit(1)
                        }
                        .buttonStyle(.plain)
                    }
                }
            }
            .navigationTitle(settings.localized("Disc Path"))
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button(settings.localized("Cancel")) { dismiss() }
                }
            }
        }
    }
}

private extension String {
    var pathExtensionLabel: String {
        let ext = (self as NSString).pathExtension.uppercased()
        return ext.isEmpty ? "FILE" : ext
    }
}
