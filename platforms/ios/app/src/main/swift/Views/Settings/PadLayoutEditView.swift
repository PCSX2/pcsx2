// PadLayoutEditView.swift — Drag-to-edit virtual pad layout
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

private struct PadLayoutEditorUndoSnapshot {
    let portrait: [String: PadGroupPosition]
    let landscape: [String: PadGroupPosition]
    let perButtonPortrait: [String: PadGroupPosition]
    let perButtonLandscape: [String: PadGroupPosition]
    let controlVisibility: [String: Bool]
    let skin: VirtualPadSkin
    let skinID: String
}

private struct PadEditorControlGeometry {
    let id: String
    let center: CGPoint
    let baseSize: CGSize
    let visibleScaleX: CGFloat
    let visibleScaleY: CGFloat
    let hitScaleX: CGFloat
    let hitScaleY: CGFloat

    var visibleSize: CGSize {
        return CGSize(
            width: PadLayoutMetrics.visibleLength(baseLength: baseSize.width, visibleScale: visibleScaleX),
            height: PadLayoutMetrics.visibleLength(baseLength: baseSize.height, visibleScale: visibleScaleY)
        )
    }

    var hitSize: CGSize {
        if isStick(id) {
            return CGSize(
                width: PadLayoutMetrics.visibleLength(baseLength: baseSize.width, visibleScale: hitScaleX),
                height: PadLayoutMetrics.visibleLength(baseLength: baseSize.height, visibleScale: hitScaleY)
            )
        }
        return CGSize(
            width: PadLayoutMetrics.touchLength(baseLength: baseSize.width, hitScale: hitScaleX),
            height: PadLayoutMetrics.touchLength(baseLength: baseSize.height, hitScale: hitScaleY)
        )
    }

    var visibleFrame: CGRect {
        CGRect(x: center.x - visibleSize.width / 2, y: center.y - visibleSize.height / 2, width: visibleSize.width, height: visibleSize.height)
    }

    var hitFrame: CGRect {
        CGRect(x: center.x - hitSize.width / 2, y: center.y - hitSize.height / 2, width: hitSize.width, height: hitSize.height)
    }
}

private enum PadLayoutNameAction: Identifiable {
    case saveNew
    case duplicate
    case rename

    var id: String {
        switch self {
        case .saveNew: return "saveNew"
        case .duplicate: return "duplicate"
        case .rename: return "rename"
        }
    }
}

struct PadLayoutEditView: View {
    let onDismiss: () -> Void
    let context: PadLayoutEditorContext
    @State private var settings = SettingsStore.shared
    @State private var layout = PadLayoutStore.shared
    @State private var layoutPresets = PadLayoutPresetStore.shared
    @State private var skinLibrary = VPadSkinLibraryStore.shared
    @State private var editLandscape = false
    @State private var undoStack: [PadLayoutEditorUndoSnapshot] = []
    @State private var originalSnapshot: PadLayoutEditorUndoSnapshot? = nil
    @State private var loadedPresetSnapshot: PadLayoutSnapshot? = nil
    @State private var activePresetID: String? = nil
    @State private var didInitializeOrientation = false
    @State private var selectedControlID: String? = nil
    @State private var pendingNameAction: PadLayoutNameAction? = nil
    @State private var nameDraft = ""
    @State private var showDeleteLayoutConfirmation = false
    @State private var layoutExportItem: ShareSheetItem?
    // Transient editor-only aspect-lock state. Not persisted: a control is
    // treated as unlinked when explicitly toggled here OR when its stored axes
    // already differ. Cleared on dismiss with the rest of the editor view state.
    @State private var unlinkedControls: Set<String> = []

    init(onDismiss: @escaping () -> Void, context: PadLayoutEditorContext = .current) {
        self.onDismiss = onDismiss
        self.context = context
    }

    private var effectiveSkinDescriptor: VPadSkinDescriptor {
        context.skinDescriptor ?? skinLibrary.selectedDescriptor
    }

    var body: some View {
        GeometryReader { geo in
            let isActuallyLandscape = geo.size.width > geo.size.height
            let isMismatch = isActuallyLandscape != editLandscape
            let portraitGameHeight = min(geo.size.width * 3 / 4, geo.size.height * 0.55)
            let editAreaW = geo.size.width
            let editAreaH = isActuallyLandscape ? geo.size.height : geo.size.height - portraitGameHeight
            ZStack(alignment: .top) {
                Color.clear
                    .contentShape(Rectangle())
                    .onTapGesture {
                        selectedControlID = nil
                    }

                if isActuallyLandscape {
                    landscapeEditorCanvas(isEditable: !isMismatch)
                } else {
                    portraitEditorCanvas(width: geo.size.width, height: geo.size.height, isEditable: !isMismatch)
                }

                VStack(spacing: 0) {
                    editorToolbar()
                        .padding(.top, max(geo.safeAreaInsets.top, 8))
                        .padding(.horizontal, max(max(geo.safeAreaInsets.leading, geo.safeAreaInsets.trailing), 8))

                    if isMismatch {
                        orientationMismatchView()
                    } else if let selectedControlID {
                        selectedControlPanel(id: selectedControlID, areaW: editAreaW, areaH: editAreaH, isLandscape: editLandscape)
                            .padding(.top, 8)
                            .padding(.horizontal, max(max(geo.safeAreaInsets.leading, geo.safeAreaInsets.trailing), 8))
                        Spacer(minLength: 0)
                    } else {
                        Spacer(minLength: 0)
                    }
                }
                .frame(width: geo.size.width, height: geo.size.height, alignment: .top)
                .position(x: geo.size.width / 2, y: geo.size.height / 2)
                .zIndex(2)
            }
            .frame(width: geo.size.width, height: geo.size.height, alignment: .top)
            .onAppear {
                if originalSnapshot == nil {
                    originalSnapshot = captureSnapshot()
                    activePresetID = context.presetID
                    if let initialSnapshot = context.initialSnapshot {
                        layout.apply(snapshot: initialSnapshot)
                        loadedPresetSnapshot = initialSnapshot
                    } else {
                        loadedPresetSnapshot = layout.snapshot()
                    }
                }
                if !didInitializeOrientation {
                    editLandscape = isActuallyLandscape
                    didInitializeOrientation = true
                }
            }
        }
        .navigationBarHidden(true)
        .statusBarHidden()
        .persistentSystemOverlays(.hidden)
        .onDisappear {
            rollbackUnsavedChanges()
            NotificationCenter.default.post(name: Notification.Name("ARMSX2iOSPadLayoutEditorDismissed"), object: nil)
        }
        .alert(nameActionTitle, isPresented: Binding(
            get: { pendingNameAction != nil },
            set: { if !$0 { pendingNameAction = nil } }
        )) {
            TextField("Layout Name", text: $nameDraft)
            Button(nameActionConfirmTitle) {
                commitPendingNameAction()
            }
            Button("Cancel", role: .cancel) {
                pendingNameAction = nil
            }
        }
        .confirmationDialog("Delete Layout?", isPresented: $showDeleteLayoutConfirmation, titleVisibility: .visible) {
            Button("Delete Layout", role: .destructive) {
                deleteCurrentPresetAndDismiss()
            }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("This deletes the layout preset. Games using it will fall back to their next available layout.")
        }
        .sheet(item: $layoutExportItem) { item in
            ActivityShareSheet(activityItems: [item.url])
        }
    }

    // MARK: - Landscape editor canvas (always shown when device is landscape)
    private func landscapeEditorCanvas(isEditable: Bool) -> some View {
        GeometryReader { geo in
            let width = geo.size.width
            let height = geo.size.height
            ZStack {
                Color.clear
                    .allowsHitTesting(false)

                if isEditable {
                    customSkinPreview(isLandscape: true, width: width, height: height)
                    editorControls(areaW: width, areaH: height, isLandscape: true)
                }
            }
            .contentShape(Rectangle())
        }
        .ignoresSafeArea()
    }

    // MARK: - Portrait editor canvas (always shown when device is portrait)
    private func portraitEditorCanvas(width: CGFloat, height: CGFloat, isEditable: Bool) -> some View {
        let gameHeight = min(width * 3 / 4, height * 0.55)
        let padHeight = height - gameHeight
        return VStack(spacing: 0) {
            // Top: reveal paused gameplay underneath
            Color.clear
                .allowsHitTesting(false)
                .frame(height: gameHeight)

            // Bottom: controller deck editing area (dark enough for controls)
            ZStack {
                Color.black.opacity(0.85)
                if isEditable {
                    customSkinPreview(isLandscape: false, width: width, height: padHeight)
                    editorControls(areaW: width, areaH: padHeight, isLandscape: false)
                }
            }
            .frame(height: padHeight)
            .clipped()
        }
    }

    private func editorToolbar() -> some View {
        HStack(spacing: 6) {
            Button {
                rollbackUnsavedChanges()
                onDismiss()
            } label: {
                Image(systemName: "xmark.circle.fill")
                    .font(.title3)
                    .foregroundStyle(.white)
                    .frame(width: 28, height: 28)
            }

            Button {
                guard let snapshot = undoStack.popLast() else { return }
                restore(snapshot: snapshot)
            } label: {
                Image(systemName: "arrow.uturn.backward.circle")
                    .font(.title3)
                    .foregroundStyle(undoStack.isEmpty ? .white.opacity(0.3) : .white)
                    .frame(width: 28, height: 28)
            }
            .disabled(undoStack.isEmpty)

            Picker("", selection: $editLandscape) {
                Text("Port.").tag(false)
                Text("Land.").tag(true)
            }
            .pickerStyle(.segmented)
            .frame(width: 112)
            .background(.black.opacity(0.3), in: RoundedRectangle(cornerRadius: 8))

            skinToolbarItem()
            layoutPresetMenuButton()
            optionsMenuButton()

            Button {
                savePrimaryAction()
            } label: {
                Image(systemName: "checkmark.circle.fill")
                    .font(.title3)
                    .foregroundStyle(.green)
                    .frame(width: 28, height: 28)
            }
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 6)
        .background(.black.opacity(0.55), in: RoundedRectangle(cornerRadius: 10, style: .continuous))
    }

    private var currentLayoutTitle: String {
        if let preset = layoutPresets.preset(id: activePresetID) {
            return preset.displayName
        }
        return "Current Layout"
    }

    private var editorIsModified: Bool {
        guard let loadedPresetSnapshot else { return false }
        return layout.snapshot() != loadedPresetSnapshot
    }

    @ViewBuilder
    private func skinToolbarItem() -> some View {
        skinPickerMenuButton()
    }

    private func layoutPresetMenuButton() -> some View {
        Menu {
            Section {
                Text(editorIsModified ? "\(currentLayoutTitle) - Modified" : currentLayoutTitle)
            }
            Button("Save as New Layout") {
                beginNameAction(.saveNew)
            }
            Button("Export Layout") {
                exportCurrentLayout()
            }
            if activePresetID != nil {
                Button("Update Current Layout") {
                    updateCurrentPresetAndDismiss()
                }
                Button("Duplicate Layout") {
                    beginNameAction(.duplicate)
                }
                Button("Rename Layout") {
                    beginNameAction(.rename)
                }
                Button("Delete Layout", role: .destructive) {
                    showDeleteLayoutConfirmation = true
                }
                Button("Set as Global Default") {
                    layoutPresets.globalPresetID = activePresetID
                }
            }
            if context.gameIdentity != nil {
                Button("Use for Current Game") {
                    assignCurrentPresetToGame()
                }
            }
        } label: {
            VStack(spacing: 0) {
                Image(systemName: editorIsModified ? "rectangle.and.pencil.and.ellipsis" : "rectangle.stack")
                    .font(.body)
                    .foregroundStyle(.white)
                Text(layoutPresets.preset(id: activePresetID)?.displayName ?? "Layout")
                    .font(.system(size: 7, weight: .semibold))
                    .foregroundStyle(.white.opacity(0.9))
                    .lineLimit(1)
                    .frame(width: 58)
            }
            .frame(width: 60, height: 28)
        }
    }

    private var nameActionTitle: String {
        switch pendingNameAction {
        case .saveNew: return "Save as New Layout"
        case .duplicate: return "Duplicate Layout"
        case .rename: return "Rename Layout"
        case nil: return "Layout"
        }
    }

    private var nameActionConfirmTitle: String {
        switch pendingNameAction {
        case .rename: return "Rename"
        case .duplicate: return "Duplicate"
        case .saveNew: return "Save"
        case nil: return "OK"
        }
    }

    private func beginNameAction(_ action: PadLayoutNameAction) {
        pendingNameAction = action
        switch action {
        case .saveNew:
            nameDraft = "Layout \(layoutPresets.presets.count + 1)"
        case .duplicate:
            nameDraft = "\(currentLayoutTitle) Copy"
        case .rename:
            nameDraft = currentLayoutTitle
        }
    }

    private func commitPendingNameAction() {
        guard let action = pendingNameAction else { return }
        let snapshot = layout.snapshot()
        switch action {
        case .saveNew:
            let preset = layoutPresets.createPreset(named: nameDraft, snapshot: snapshot)
            activePresetID = preset.id
            loadedPresetSnapshot = snapshot
            if let identity = context.gameIdentity {
                layoutPresets.setPreset(preset.id, for: identity)
            }
            finishPresetEditRestoringOriginal()
        case .duplicate:
            if let activePresetID,
               let duplicate = try? layoutPresets.duplicatePreset(id: activePresetID, named: nameDraft) {
                self.activePresetID = duplicate.id
                loadedPresetSnapshot = duplicate.snapshot
                if let identity = context.gameIdentity {
                    layoutPresets.setPreset(duplicate.id, for: identity)
                }
            }
        case .rename:
            if let activePresetID {
                try? layoutPresets.renamePreset(id: activePresetID, to: nameDraft)
            }
        }
        pendingNameAction = nil
    }

    private func savePrimaryAction() {
        // Only create or update a layout preset when the layout geometry actually
        // changed. Skin-only preview/reset must not force "Save as New Layout".
        guard editorIsModified else {
            undoStack.removeAll()
            originalSnapshot = nil
            onDismiss()
            return
        }

        if let activePresetID {
            updateCurrentPresetAndDismiss()
        } else if context.gameIdentity != nil || context.initialSnapshot != nil {
            beginNameAction(.saveNew)
        } else {
            layout.save()
            undoStack.removeAll()
            originalSnapshot = nil
            onDismiss()
        }
    }

    private func updateCurrentPresetAndDismiss() {
        guard let activePresetID else {
            beginNameAction(.saveNew)
            return
        }
        let snapshot = layout.snapshot()
        try? layoutPresets.updatePreset(id: activePresetID, snapshot: snapshot)
        loadedPresetSnapshot = snapshot
        finishPresetEditRestoringOriginal()
    }

    private func deleteCurrentPresetAndDismiss() {
        guard let activePresetID else { return }
        try? layoutPresets.deletePreset(id: activePresetID)
        finishPresetEditRestoringOriginal()
    }

    private func assignCurrentPresetToGame() {
        guard let identity = context.gameIdentity else { return }
        if let activePresetID {
            layoutPresets.setPreset(activePresetID, for: identity)
        } else {
            beginNameAction(.saveNew)
        }
    }

    private func exportCurrentLayout() {
        do {
            let data = try PadLayoutImportExport.exportData(
                displayName: currentLayoutTitle,
                snapshot: layout.snapshot()
            )
            let url = FileManager.default.temporaryDirectory
                .appendingPathComponent(PadLayoutImportExport.exportedFileName(for: currentLayoutTitle))
            try data.write(to: url, options: .atomic)
            layoutExportItem = ShareSheetItem(url: url)
        } catch {
            NSLog("[ARMSX2 iOS Layout] export failed: %@", error.localizedDescription)
        }
    }

    private func finishPresetEditRestoringOriginal() {
        if let originalSnapshot {
            // Roll back the temporary layout working copy while keeping any skin
            // change made inside the editor (skin reset/selection is skin-only).
            let currentSkin = settings.virtualPadSkin
            let currentSkinID = skinLibrary.selectedSkinID
            restore(snapshot: originalSnapshot)
            settings.virtualPadSkin = currentSkin
            skinLibrary.selectSkin(id: currentSkinID)
        }
        undoStack.removeAll()
        originalSnapshot = nil
        onDismiss()
    }

    private func orientationMismatchView() -> some View {
        let message = editLandscape
            ? "Rotate device to landscape to edit landscape layout"
            : "Rotate device to portrait to edit portrait layout"
        let explanation = editLandscape
            ? "Accurate editing requires rotating the device or switching to portrait layout."
            : "Accurate editing requires rotating the device or switching to landscape layout."
        return VStack(spacing: 12) {
            Image(systemName: "rotate.right")
                .font(.system(size: 48, weight: .light))
                .foregroundStyle(.white.opacity(0.6))
            Text(message)
                .font(.headline.weight(.semibold))
                .foregroundStyle(.white)
                .multilineTextAlignment(.center)
                .lineLimit(3)
            Text(explanation)
                .font(.subheadline)
                .foregroundStyle(.white.opacity(0.7))
                .multilineTextAlignment(.center)
                .lineLimit(3)
        }
        .padding(.horizontal, 32)
        .padding(.vertical, 24)
        .background(.black.opacity(0.6), in: RoundedRectangle(cornerRadius: 16))
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .allowsHitTesting(false)
    }

    private func skinPickerMenuButton() -> some View {
        Menu {
            ForEach(skinLibrary.allDescriptors) { skin in
                Button {
                    guard skinLibrary.selectedSkinID != skin.id else { return }
                    pushSnapshot()
                    skinLibrary.selectSkin(id: skin.id)
                    settings.virtualPadSkin = skin.virtualPadSkin
                } label: {
                    Label(settings.localized(skin.displayName), systemImage: skinLibrary.selectedSkinID == skin.id ? "checkmark" : "circle")
                }
            }
        } label: {
            Image(systemName: "paintpalette.fill")
                .font(.title3)
                .foregroundStyle(.white)
                .frame(width: 28, height: 28)
        }
    }

    private func optionsMenuButton() -> some View {
        Menu {
            Section("Visibility") {
                ForEach(PadLayoutStore.groupIDs.filter { $0 != "action" }, id: \.self) { (id: String) in
                    visibilityMenuItem(id: id)
                }
                visibilityMenuItem(id: "cross", label: "X")
                visibilityMenuItem(id: "circle", label: "O")
                visibilityMenuItem(id: "triangle", label: "Triangle")
                visibilityMenuItem(id: "square", label: "Square")
            }

            Section("Reset") {
                Button("Reset Action", role: .destructive) {
                    pushSnapshot()
                    layout.resetPerButtonActionButtons(isLandscape: editLandscape)
                }
                Button("Reset D-Pad", role: .destructive) {
                    pushSnapshot()
                    layout.resetPerButtonDPad(isLandscape: editLandscape)
                }
                Button("Reset All", role: .destructive) {
                    pushSnapshot()
                    layout.resetAll()
                }
                Button("Reset Visibility", role: .destructive) {
                    pushSnapshot()
                    layout.resetControlVisibility()
                }
            }
        } label: {
            Image(systemName: "ellipsis.circle")
                .font(.title3)
                .foregroundStyle(.white)
                .frame(width: 28, height: 28)
        }
    }

    private func visibilityMenuItem(id: String, label: String? = nil) -> some View {
        let visible = layout.isControlVisible(id)
        return Button {
            pushSnapshot()
            layout.setControlVisible(id, visible: !visible)
        } label: {
            let name = label ?? id.uppercased()
            Label(name, systemImage: visible ? "eye" : "eye.slash")
        }
    }

    /// A control is shown unlinked when explicitly toggled or when its stored
    /// axes already differ (inferred). Sticks are always linked (no per-axis).
    private func isUnlinked(id: String, position: PadGroupPosition) -> Bool {
        guard !isStick(id) else { return false }
        if unlinkedControls.contains(id) { return true }
        return position.scaleX != position.scaleY || position.hitScaleX != position.hitScaleY
    }

    @ViewBuilder
    private func selectedControlPanel(id: String, areaW: CGFloat, areaH: CGFloat, isLandscape: Bool) -> some View {
        let position = selectedPosition(id: id, areaW: areaW, areaH: areaH, isLandscape: isLandscape)
        let unlinked = isUnlinked(id: id, position: position)
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 10) {
                Text(editorLabel(for: id))
                    .font(.caption.weight(.bold))
                    .foregroundStyle(.white)
                    .frame(minWidth: 54, alignment: .leading)

                if !isStick(id) {
                    aspectLockButton(id: id, unlinked: unlinked, areaW: areaW, areaH: areaH, isLandscape: isLandscape)
                }

                if !unlinked {
                    // Linked: one Visible + one Hit stepper drive both axes.
                    scaleStepper(
                        title: "Visible",
                        value: position.scale,
                        canDecrease: position.scale > PadLayoutMetrics.minControlScale,
                        canIncrease: position.scale < PadLayoutMetrics.maxControlScale,
                        onDecrease: { adjustSelectedVisibleScale(id: id, by: -0.1, areaW: areaW, areaH: areaH, isLandscape: isLandscape) },
                        onIncrease: { adjustSelectedVisibleScale(id: id, by: 0.1, areaW: areaW, areaH: areaH, isLandscape: isLandscape) }
                    )

                    if !isStick(id) {
                        scaleStepper(
                            title: "Hit",
                            value: position.hitScale,
                            canDecrease: position.hitScale > PadLayoutMetrics.minControlScale,
                            canIncrease: position.hitScale < PadLayoutMetrics.maxControlScale,
                            onDecrease: { adjustSelectedHitScale(id: id, by: -0.1, areaW: areaW, areaH: areaH, isLandscape: isLandscape) },
                            onIncrease: { adjustSelectedHitScale(id: id, by: 0.1, areaW: areaW, areaH: areaH, isLandscape: isLandscape) }
                        )
                    }
                }
            }

            if unlinked {
                // Unlinked: width/height steppers for visible and hit, two rows.
                HStack(spacing: 10) {
                    axisStepper(title: "Vis W", value: position.scaleX, id: id, kind: .visible, axis: .x, areaW: areaW, areaH: areaH, isLandscape: isLandscape)
                    axisStepper(title: "Vis H", value: position.scaleY, id: id, kind: .visible, axis: .y, areaW: areaW, areaH: areaH, isLandscape: isLandscape)
                }
                HStack(spacing: 10) {
                    axisStepper(title: "Hit W", value: position.hitScaleX, id: id, kind: .hit, axis: .x, areaW: areaW, areaH: areaH, isLandscape: isLandscape)
                    axisStepper(title: "Hit H", value: position.hitScaleY, id: id, kind: .hit, axis: .y, areaW: areaW, areaH: areaH, isLandscape: isLandscape)
                }
            }
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 7)
        .background(.black.opacity(0.58), in: RoundedRectangle(cornerRadius: 10, style: .continuous))
    }

    private func aspectLockButton(id: String, unlinked: Bool, areaW: CGFloat, areaH: CGFloat, isLandscape: Bool) -> some View {
        Button {
            if unlinked {
                // Re-lock: collapse to width (width-wins) and record link state.
                pushSnapshot()
                let perButton = PadLayoutStore.perButtonIDs.contains(id)
                layout.relinkAxes(id, perButton: perButton, landscape: isLandscape, areaW: areaW, areaH: areaH)
                unlinkedControls.remove(id)
            } else {
                // Unlock: reveal per-axis steppers. No data change yet.
                unlinkedControls.insert(id)
            }
        } label: {
            Image(systemName: unlinked ? "lock.open" : "lock")
                .font(.caption)
                .foregroundStyle(unlinked ? .yellow : .white)
                .frame(width: 24, height: 24)
        }
        .accessibilityLabel(unlinked ? "Unlinked width and height" : "Linked width and height")
    }

    private func axisStepper(title: String, value: CGFloat, id: String, kind: PadSizeKind, axis: PadSizeAxis, areaW: CGFloat, areaH: CGFloat, isLandscape: Bool) -> some View {
        scaleStepper(
            title: title,
            value: value,
            canDecrease: value > PadLayoutMetrics.minControlScale,
            canIncrease: value < PadLayoutMetrics.maxControlScale,
            onDecrease: { adjustSelectedAxis(id: id, kind: kind, axis: axis, by: -0.1, areaW: areaW, areaH: areaH, isLandscape: isLandscape) },
            onIncrease: { adjustSelectedAxis(id: id, kind: kind, axis: axis, by: 0.1, areaW: areaW, areaH: areaH, isLandscape: isLandscape) }
        )
    }

    private func scaleStepper(
        title: String,
        value: CGFloat,
        canDecrease: Bool,
        canIncrease: Bool,
        onDecrease: @escaping () -> Void,
        onIncrease: @escaping () -> Void
    ) -> some View {
        HStack(spacing: 5) {
            Text(title)
                .font(.caption2.weight(.semibold))
                .foregroundStyle(.white.opacity(0.75))
            Button(action: onDecrease) {
                Image(systemName: "minus.circle.fill")
                    .font(.body)
                    .foregroundStyle(canDecrease ? .white : .white.opacity(0.3))
                    .frame(width: 24, height: 24)
            }
            .disabled(!canDecrease)
            Text(String(format: "%.1f", Double(value)))
                .font(.caption.monospacedDigit().weight(.semibold))
                .foregroundStyle(.white)
                .frame(width: 30)
            Button(action: onIncrease) {
                Image(systemName: "plus.circle.fill")
                    .font(.body)
                    .foregroundStyle(canIncrease ? .white : .white.opacity(0.3))
                    .frame(width: 24, height: 24)
            }
            .disabled(!canIncrease)
        }
    }

    private func selectedPosition(id: String, areaW: CGFloat, areaH: CGFloat, isLandscape: Bool) -> PadGroupPosition {
        if PadLayoutStore.perButtonIDs.contains(id) {
            return layout.perButtonPosition(for: id, landscape: isLandscape, areaW: areaW, areaH: areaH)
        }
        return layout.position(for: id, landscape: isLandscape)
    }

    private func adjustSelectedVisibleScale(id: String, by delta: CGFloat, areaW: CGFloat, areaH: CGFloat, isLandscape: Bool) {
        let current = selectedPosition(id: id, areaW: areaW, areaH: areaH, isLandscape: isLandscape).scale
        let next = PadLayoutMetrics.clampedScale(current + delta)
        guard next != current else { return }
        pushSnapshot()
        if PadLayoutStore.perButtonIDs.contains(id) {
            layout.updatePerButtonScale(id, scale: next, landscape: isLandscape, areaW: areaW, areaH: areaH)
        } else {
            layout.updateGroupScale(id, scale: next, landscape: isLandscape)
        }
    }

    private func adjustSelectedHitScale(id: String, by delta: CGFloat, areaW: CGFloat, areaH: CGFloat, isLandscape: Bool) {
        guard !isStick(id) else { return }
        let current = selectedPosition(id: id, areaW: areaW, areaH: areaH, isLandscape: isLandscape).hitScale
        let next = PadLayoutMetrics.clampedScale(current + delta)
        guard next != current else { return }
        pushSnapshot()
        if PadLayoutStore.perButtonIDs.contains(id) {
            layout.updatePerButtonHitScale(id, hitScale: next, landscape: isLandscape, areaW: areaW, areaH: areaH)
        } else {
            layout.updateGroupHitScale(id, hitScale: next, landscape: isLandscape)
        }
    }

    private func adjustSelectedAxis(id: String, kind: PadSizeKind, axis: PadSizeAxis, by delta: CGFloat, areaW: CGFloat, areaH: CGFloat, isLandscape: Bool) {
        guard !isStick(id) else { return }
        let position = selectedPosition(id: id, areaW: areaW, areaH: areaH, isLandscape: isLandscape)
        let current = axisValue(of: position, kind: kind, axis: axis)
        let next = PadLayoutMetrics.clampedScale(current + delta)
        guard next != current else { return }
        pushSnapshot()
        if PadLayoutStore.perButtonIDs.contains(id) {
            layout.updatePerButtonSize(id, kind: kind, axis: axis, scale: next, landscape: isLandscape, areaW: areaW, areaH: areaH)
        } else {
            layout.updateGroupSize(id, kind: kind, axis: axis, scale: next, landscape: isLandscape)
        }
    }

    private func axisValue(of position: PadGroupPosition, kind: PadSizeKind, axis: PadSizeAxis) -> CGFloat {
        switch (kind, axis) {
        case (.visible, .x): return position.scaleX
        case (.visible, .y): return position.scaleY
        case (.hit, .x):     return position.hitScaleX
        case (.hit, .y):     return position.hitScaleY
        }
    }

    @ViewBuilder
    private func customSkinPreview(isLandscape: Bool, width: CGFloat, height: CGFloat) -> some View {
        if let fullSkin = ControllerAsset.gameplayFullSkinImage(descriptor: effectiveSkinDescriptor, isLandscape: isLandscape) {
            Image(uiImage: fullSkin)
                .resizable()
                .interpolation(.high)
                .antialiased(true)
                .scaledToFill()
                .frame(width: width, height: height)
                .clipped()
                .opacity(0.42)
                .allowsHitTesting(false)
        }
    }

    @ViewBuilder
    private func editorControls(areaW: CGFloat, areaH: CGFloat, isLandscape: Bool) -> some View {
        let stickScale = min(max(CGFloat(settings.analogStickScale), 0.8), 1.6)
        let overlaps = overlappingControlIDs(in: editorControlGeometries(areaW: areaW, areaH: areaH, isLandscape: isLandscape, analogStickScale: stickScale))
        ZStack {
            ForEach(PadLayoutStore.perButtonIDs, id: \.self) { (id: String) in
                DraggableButton(
                    id: id,
                    areaW: areaW,
                    areaH: areaH,
                    isLandscape: isLandscape,
                    selectedControlID: $selectedControlID,
                    isSelected: selectedControlID == id,
                    isOverlapping: overlaps.contains(id),
                    onBeginEdit: pushSnapshot
                )
            }
            ForEach(PadLayoutStore.groupIDs.filter { $0 != "action" && $0 != "dpad" }, id: \.self) { (id: String) in
                DraggableGroup(
                    id: id,
                    areaW: areaW,
                    areaH: areaH,
                    isLandscape: isLandscape,
                    analogStickScale: stickScale,
                    selectedControlID: $selectedControlID,
                    isSelected: selectedControlID == id,
                    isOverlapping: overlaps.contains(id),
                    onBeginEdit: pushSnapshot
                )
            }
        }
        .environment(\.padSkin, effectiveSkinDescriptor.virtualPadSkin)
        .environment(\.padSkinDescriptor, effectiveSkinDescriptor)
        .environment(\.padOpacity, 1.0)
        .environment(\.padUsesFullSkin, false)
    }

    private func editorControlGeometries(areaW: CGFloat, areaH: CGFloat, isLandscape: Bool, analogStickScale: CGFloat) -> [PadEditorControlGeometry] {
        let perButtonGeometries = PadLayoutStore.perButtonIDs.compactMap { id -> PadEditorControlGeometry? in
            guard layout.isControlVisible(id) else { return nil }
            let position = layout.perButtonPosition(for: id, landscape: isLandscape, areaW: areaW, areaH: areaH)
            return PadEditorControlGeometry(
                id: id,
                center: CGPoint(x: position.x * areaW, y: position.y * areaH),
                baseSize: padEditorBaseSize(for: id, isLandscape: isLandscape, analogStickScale: analogStickScale),
                visibleScaleX: position.scaleX,
                visibleScaleY: position.scaleY,
                hitScaleX: position.hitScaleX,
                hitScaleY: position.hitScaleY
            )
        }

        let groupGeometries = PadLayoutStore.groupIDs
            .filter { $0 != "action" && $0 != "dpad" }
            .compactMap { id -> PadEditorControlGeometry? in
                guard layout.isControlVisible(id) else { return nil }
                let position = layout.position(for: id, landscape: isLandscape)
                // Sticks keep coupled (square) hit sizing tied to the visible scale.
                let hitX = isStick(id) ? position.scaleX : position.hitScaleX
                let hitY = isStick(id) ? position.scaleY : position.hitScaleY
                return PadEditorControlGeometry(
                    id: id,
                    center: CGPoint(x: position.x * areaW, y: position.y * areaH),
                    baseSize: padEditorBaseSize(for: id, isLandscape: isLandscape, analogStickScale: analogStickScale),
                    visibleScaleX: position.scaleX,
                    visibleScaleY: position.scaleY,
                    hitScaleX: hitX,
                    hitScaleY: hitY
                )
            }

        return perButtonGeometries + groupGeometries
    }

    private func overlappingControlIDs(in geometries: [PadEditorControlGeometry]) -> Set<String> {
        var result = Set<String>()
        for leftIndex in geometries.indices {
            for rightIndex in geometries.index(after: leftIndex)..<geometries.endIndex {
                let left = geometries[leftIndex]
                let right = geometries[rightIndex]
                guard left.hitFrame.intersects(right.hitFrame) else { continue }
                let overlap = left.hitFrame.intersection(right.hitFrame)
                if overlap.width > 0 && overlap.height > 0 {
                    result.insert(left.id)
                    result.insert(right.id)
                }
            }
        }
        return result
    }

    // MARK: - Snapshot / Undo

    private func captureSnapshot() -> PadLayoutEditorUndoSnapshot {
        PadLayoutEditorUndoSnapshot(
            portrait: layout.portrait,
            landscape: layout.landscape,
            perButtonPortrait: layout.perButtonPortrait,
            perButtonLandscape: layout.perButtonLandscape,
            controlVisibility: layout.controlVisibility,
            skin: settings.virtualPadSkin,
            skinID: skinLibrary.selectedSkinID
        )
    }

    private func restore(snapshot: PadLayoutEditorUndoSnapshot) {
        layout.portrait = snapshot.portrait
        layout.landscape = snapshot.landscape
        layout.perButtonPortrait = snapshot.perButtonPortrait
        layout.perButtonLandscape = snapshot.perButtonLandscape
        layout.controlVisibility = snapshot.controlVisibility
        skinLibrary.selectSkin(id: snapshot.skinID)
        settings.virtualPadSkin = snapshot.skin
    }

    private func pushSnapshot() {
        undoStack.append(captureSnapshot())
    }

    private func rollbackUnsavedChanges() {
        guard let snapshot = originalSnapshot else { return }
        restore(snapshot: snapshot)
        undoStack.removeAll()
        originalSnapshot = nil
    }
}

private func isStick(_ id: String) -> Bool {
    id == "lstick" || id == "rstick"
}

private func editorLabel(for id: String) -> String {
    switch id {
    case "up": return "UP"
    case "down": return "DN"
    case "left": return "LT"
    case "right": return "RT"
    case "triangle": return "TRI"
    case "circle": return "O"
    case "square": return "SQR"
    case "cross": return "X"
    case "select": return "SEL"
    default: return id.uppercased()
    }
}

private func padEditorBaseSize(for id: String, isLandscape: Bool, analogStickScale: CGFloat) -> CGSize {
    switch id {
    case "up", "down", "left", "right":
        let width = VirtualPadButtonOffset.dpadButtonWidth(isLandscape: isLandscape)
        return CGSize(width: width, height: width)
    case "triangle", "circle", "square", "cross":
        let size = VirtualPadButtonOffset.actionButtonSize
        return CGSize(width: size, height: size)
    case "l1", "r1":
        return isLandscape ? CGSize(width: 120, height: 32) : CGSize(width: 100, height: 30)
    case "l2", "r2":
        return isLandscape ? CGSize(width: 130, height: 44) : CGSize(width: 110, height: 40)
    case "lstick", "rstick":
        let size = 68 * min(max(analogStickScale, 0.8), 1.6)
        return CGSize(width: size, height: size)
    case "select":
        return CGSize(width: isLandscape ? 40 : 42, height: 22)
    case "start":
        return CGSize(width: 48, height: 22)
    default:
        return CGSize(width: 60, height: 40)
    }
}

// MARK: - Draggable group widget
private struct DraggableGroup: View {
    let id: String
    let areaW: CGFloat
    let areaH: CGFloat
    let isLandscape: Bool
    let analogStickScale: CGFloat
    @Binding var selectedControlID: String?
    let isSelected: Bool
    let isOverlapping: Bool
    let onBeginEdit: () -> Void

    @State private var layout = PadLayoutStore.shared
    @State private var dragOffset: CGSize = .zero
    @State private var currentScale: CGFloat = 1.0
    @State private var hasPushedSnapshot = false

    private var pos: PadGroupPosition {
        layout.position(for: id, landscape: isLandscape)
    }

    private var currentX: CGFloat { pos.x * areaW + dragOffset.width }
    private var currentY: CGFloat { pos.y * areaH + dragOffset.height }
    // Pinch multiplies both axes by the same factor (ratio-preserving / uniform).
    private var visibleScaleX: CGFloat {
        PadLayoutMetrics.clampedScale(pos.scaleX * currentScale)
    }
    private var visibleScaleY: CGFloat {
        PadLayoutMetrics.clampedScale(pos.scaleY * currentScale)
    }
    private var hitScaleX: CGFloat {
        isStick(id) ? visibleScaleX : pos.hitScaleX
    }
    private var hitScaleY: CGFloat {
        isStick(id) ? visibleScaleY : pos.hitScaleY
    }
    private var visibleSize: CGSize {
        CGSize(
            width: PadLayoutMetrics.visibleLength(baseLength: baseSize.width, visibleScale: visibleScaleX),
            height: PadLayoutMetrics.visibleLength(baseLength: baseSize.height, visibleScale: visibleScaleY)
        )
    }
    private var hitSize: CGSize {
        if isStick(id) {
            return visibleSize
        }
        return CGSize(
            width: PadLayoutMetrics.touchLength(baseLength: baseSize.width, hitScale: hitScaleX),
            height: PadLayoutMetrics.touchLength(baseLength: baseSize.height, hitScale: hitScaleY)
        )
    }

    var body: some View {
        let isVisible = layout.isControlVisible(id)
        ZStack {
            RoundedRectangle(cornerRadius: 8)
                .fill(.white.opacity(isSelected ? 0.10 : 0.04))
                .frame(width: visibleSize.width + 12, height: visibleSize.height + 12)
                .overlay {
                    RoundedRectangle(cornerRadius: 8)
                        .stroke(.blue.opacity(isSelected ? 0.95 : 0.45), lineWidth: isSelected ? 2.4 : 1.4)
                }

            RoundedRectangle(cornerRadius: 8)
                .stroke((isOverlapping ? Color.red : Color.yellow).opacity(isSelected || isOverlapping ? 0.92 : 0.42), style: StrokeStyle(lineWidth: isSelected || isOverlapping ? 2.0 : 1.2, dash: [6, 4]))
                .frame(width: hitSize.width, height: hitSize.height)

            groupContent
                .allowsHitTesting(false)

            Text(id.uppercased())
                .font(.system(size: 9, weight: .bold))
                .foregroundStyle((isOverlapping ? Color.red : Color.blue).opacity(isSelected ? 1.0 : 0.75))
                .offset(y: -(visibleSize.height / 2 + 12))
        }
        .frame(width: hitSize.width, height: hitSize.height)
        .contentShape(Rectangle())
        .position(x: currentX, y: currentY)
        .opacity(isVisible ? 0.8 : 0.25)
        .gesture(
            DragGesture(minimumDistance: 0)
                .onChanged { v in
                    selectedControlID = id
                    let hasMoved = abs(v.translation.width) > 1 || abs(v.translation.height) > 1
                    if hasMoved && !hasPushedSnapshot {
                        onBeginEdit()
                        hasPushedSnapshot = true
                    }
                    dragOffset = v.translation
                }
                .onEnded { v in
                    selectedControlID = id
                    let hasMoved = abs(v.translation.width) > 1 || abs(v.translation.height) > 1
                    hasPushedSnapshot = false
                    if hasMoved {
                        let newX = (pos.x * areaW + v.translation.width) / areaW
                        let newY = (pos.y * areaH + v.translation.height) / areaH
                        updatePosition(x: newX.clamped(0, 1), y: newY.clamped(0, 1))
                    }
                    dragOffset = .zero
                }
        )
        .simultaneousGesture(
            MagnifyGesture()
                .onChanged { v in
                    if !hasPushedSnapshot {
                        onBeginEdit()
                        hasPushedSnapshot = true
                    }
                    currentScale = v.magnification
                }
                .onEnded { v in
                    hasPushedSnapshot = false
                    updateScale(magnification: v.magnification)
                    currentScale = 1.0
                }
        )
    }

    private func updatePosition(x: CGFloat, y: CGFloat) {
        var p = pos
        p.x = x
        p.y = y
        if isLandscape {
            layout.landscape[id] = p
        } else {
            layout.portrait[id] = p
        }
    }

    private func updateScale(magnification: CGFloat) {
        // Uniform pinch: scale both visible axes by the same factor, preserving
        // any existing width/height ratio.
        var p = pos
        p.scaleX = PadLayoutMetrics.clampedScale(p.scaleX * magnification)
        p.scaleY = PadLayoutMetrics.clampedScale(p.scaleY * magnification)
        if isLandscape {
            layout.landscape[id] = p
        } else {
            layout.portrait[id] = p
        }
    }

    private var baseSize: CGSize {
        padEditorBaseSize(for: id, isLandscape: isLandscape, analogStickScale: analogStickScale)
    }

    @ViewBuilder
    private var groupContent: some View {
        switch id {
        case "dpad":
            DPadView(size: isLandscape ? VirtualPadButtonOffset.dpadLandscapeSize : VirtualPadButtonOffset.dpadPortraitSize)
        case "action":
            ActionButtonsView(size: VirtualPadButtonOffset.actionButtonSize)
        case "l1":
            PadBtn(label: "L1", w: baseSize.width, h: baseSize.height, btn: .L1, visibleScaleX: visibleScaleX, visibleScaleY: visibleScaleY, hitScaleX: hitScaleX, hitScaleY: hitScaleY)
        case "l2":
            PadBtn(label: "L2", w: baseSize.width, h: baseSize.height, btn: .L2, visibleScaleX: visibleScaleX, visibleScaleY: visibleScaleY, hitScaleX: hitScaleX, hitScaleY: hitScaleY)
        case "r1":
            PadBtn(label: "R1", w: baseSize.width, h: baseSize.height, btn: .R1, visibleScaleX: visibleScaleX, visibleScaleY: visibleScaleY, hitScaleX: hitScaleX, hitScaleY: hitScaleY)
        case "r2":
            PadBtn(label: "R2", w: baseSize.width, h: baseSize.height, btn: .R2, visibleScaleX: visibleScaleX, visibleScaleY: visibleScaleY, hitScaleX: hitScaleX, hitScaleY: hitScaleY)
        case "lstick":
            StickView(isLeft: true, sizeScale: analogStickScale, layoutScale: visibleScaleX)
        case "rstick":
            StickView(isLeft: false, sizeScale: analogStickScale, layoutScale: visibleScaleX)
        case "select":
            PadBtn(label: "SEL", w: baseSize.width, h: baseSize.height, btn: .select, visibleScaleX: visibleScaleX, visibleScaleY: visibleScaleY, hitScaleX: hitScaleX, hitScaleY: hitScaleY)
        case "start":
            PadBtn(label: "START", w: baseSize.width, h: baseSize.height, btn: .start, visibleScaleX: visibleScaleX, visibleScaleY: visibleScaleY, hitScaleX: hitScaleX, hitScaleY: hitScaleY)
        default:
            EmptyView()
        }
    }
}

// MARK: - Draggable individual button widget
private struct DraggableButton: View {
    let id: String
    let areaW: CGFloat
    let areaH: CGFloat
    let isLandscape: Bool
    @Binding var selectedControlID: String?
    let isSelected: Bool
    let isOverlapping: Bool
    let onBeginEdit: () -> Void

    @State private var layout = PadLayoutStore.shared
    @State private var dragOffset: CGSize = .zero
    @State private var currentScale: CGFloat = 1.0
    @State private var hasPushedSnapshot = false

    private var pos: PadGroupPosition {
        layout.perButtonPosition(for: id, landscape: isLandscape, areaW: areaW, areaH: areaH)
    }

    private var currentX: CGFloat { pos.x * areaW + dragOffset.width }
    private var currentY: CGFloat { pos.y * areaH + dragOffset.height }

    // Pinch multiplies both axes by the same factor (ratio-preserving / uniform).
    private var visibleScaleX: CGFloat {
        PadLayoutMetrics.clampedScale(pos.scaleX * currentScale)
    }
    private var visibleScaleY: CGFloat {
        PadLayoutMetrics.clampedScale(pos.scaleY * currentScale)
    }
    private var visibleSize: CGSize {
        CGSize(
            width: PadLayoutMetrics.visibleLength(baseLength: baseSize.width, visibleScale: visibleScaleX),
            height: PadLayoutMetrics.visibleLength(baseLength: baseSize.height, visibleScale: visibleScaleY)
        )
    }
    private var hitSize: CGSize {
        CGSize(
            width: PadLayoutMetrics.touchLength(baseLength: baseSize.width, hitScale: pos.hitScaleX),
            height: PadLayoutMetrics.touchLength(baseLength: baseSize.height, hitScale: pos.hitScaleY)
        )
    }

    var body: some View {
        let isVisible = layout.isControlVisible(id)
        ZStack {
            RoundedRectangle(cornerRadius: 6)
                .fill(.white.opacity(isSelected ? 0.10 : 0.04))
                .frame(width: visibleSize.width + 10, height: visibleSize.height + 10)
                .overlay {
                    RoundedRectangle(cornerRadius: 6)
                        .stroke(.blue.opacity(isSelected ? 0.95 : 0.45), lineWidth: isSelected ? 2.3 : 1.3)
                }

            RoundedRectangle(cornerRadius: 6)
                .stroke((isOverlapping ? Color.red : Color.yellow).opacity(isSelected || isOverlapping ? 0.92 : 0.42), style: StrokeStyle(lineWidth: isSelected || isOverlapping ? 2.0 : 1.2, dash: [6, 4]))
                .frame(width: hitSize.width, height: hitSize.height)

            buttonContent
                .allowsHitTesting(false)

            Text(buttonLabel.uppercased())
                .font(.system(size: 8, weight: .bold))
                .foregroundStyle((isOverlapping ? Color.red : Color.blue).opacity(isSelected ? 1.0 : 0.75))
                .offset(y: -(visibleSize.height / 2 + 10))
        }
        .frame(width: hitSize.width, height: hitSize.height)
        .contentShape(Rectangle())
        .position(x: currentX, y: currentY)
        .opacity(isVisible ? 0.8 : 0.25)
        .gesture(
            DragGesture(minimumDistance: 0)
                .onChanged { v in
                    selectedControlID = id
                    let hasMoved = abs(v.translation.width) > 1 || abs(v.translation.height) > 1
                    if hasMoved && !hasPushedSnapshot {
                        onBeginEdit()
                        hasPushedSnapshot = true
                    }
                    dragOffset = v.translation
                }
                .onEnded { v in
                    selectedControlID = id
                    let hasMoved = abs(v.translation.width) > 1 || abs(v.translation.height) > 1
                    hasPushedSnapshot = false
                    if hasMoved {
                        let newX = (pos.x * areaW + v.translation.width) / areaW
                        let newY = (pos.y * areaH + v.translation.height) / areaH
                        updatePosition(x: newX.clamped(0, 1), y: newY.clamped(0, 1))
                    }
                    dragOffset = .zero
                }
        )
        .simultaneousGesture(
            MagnifyGesture()
                .onChanged { v in
                    if !hasPushedSnapshot {
                        onBeginEdit()
                        hasPushedSnapshot = true
                    }
                    currentScale = v.magnification
                }
                .onEnded { v in
                    hasPushedSnapshot = false
                    updateScale(magnification: v.magnification)
                    currentScale = 1.0
                }
        )
    }

    private var buttonLabel: String {
        switch id {
        case "up": return "UP"
        case "down": return "DN"
        case "left": return "LT"
        case "right": return "RT"
        case "triangle": return "TRI"
        case "circle": return "CIR"
        case "square": return "SQU"
        case "cross": return "CRO"
        default: return id
        }
    }

    private var baseSize: CGSize {
        padEditorBaseSize(for: id, isLandscape: isLandscape, analogStickScale: 1.0)
    }

    private func updatePosition(x: CGFloat, y: CGFloat) {
        var p = pos
        p.x = x
        p.y = y
        if isLandscape {
            layout.perButtonLandscape[id] = p
        } else {
            layout.perButtonPortrait[id] = p
        }
    }

    private func updateScale(magnification: CGFloat) {
        // Uniform pinch: scale both visible axes by the same factor, preserving
        // any existing width/height ratio.
        var p = pos
        p.scaleX = PadLayoutMetrics.clampedScale(p.scaleX * magnification)
        p.scaleY = PadLayoutMetrics.clampedScale(p.scaleY * magnification)
        if isLandscape {
            layout.perButtonLandscape[id] = p
        } else {
            layout.perButtonPortrait[id] = p
        }
    }

    @ViewBuilder
    private var buttonContent: some View {
        switch id {
        case "up":
            PadBtn(label: "▲", w: baseSize.width, h: baseSize.height, btn: .up, visibleScaleX: visibleScaleX, visibleScaleY: visibleScaleY, hitScaleX: pos.hitScaleX, hitScaleY: pos.hitScaleY)
        case "down":
            PadBtn(label: "▼", w: baseSize.width, h: baseSize.height, btn: .down, visibleScaleX: visibleScaleX, visibleScaleY: visibleScaleY, hitScaleX: pos.hitScaleX, hitScaleY: pos.hitScaleY)
        case "left":
            PadBtn(label: "◀", w: baseSize.width, h: baseSize.height, btn: .left, visibleScaleX: visibleScaleX, visibleScaleY: visibleScaleY, hitScaleX: pos.hitScaleX, hitScaleY: pos.hitScaleY)
        case "right":
            PadBtn(label: "▶", w: baseSize.width, h: baseSize.height, btn: .right, visibleScaleX: visibleScaleX, visibleScaleY: visibleScaleY, hitScaleX: pos.hitScaleX, hitScaleY: pos.hitScaleY)
        case "triangle":
            PSBtn(sym: "△", clr: .green, sz: baseSize.width, btn: .triangle, visibleScaleX: visibleScaleX, visibleScaleY: visibleScaleY, hitScaleX: pos.hitScaleX, hitScaleY: pos.hitScaleY)
        case "cross":
            PSBtn(sym: "✕", clr: .blue, sz: baseSize.width, btn: .cross, visibleScaleX: visibleScaleX, visibleScaleY: visibleScaleY, hitScaleX: pos.hitScaleX, hitScaleY: pos.hitScaleY)
        case "square":
            PSBtn(sym: "□", clr: .pink, sz: baseSize.width, btn: .square, visibleScaleX: visibleScaleX, visibleScaleY: visibleScaleY, hitScaleX: pos.hitScaleX, hitScaleY: pos.hitScaleY)
        case "circle":
            PSBtn(sym: "○", clr: .red, sz: baseSize.width, btn: .circle, visibleScaleX: visibleScaleX, visibleScaleY: visibleScaleY, hitScaleX: pos.hitScaleX, hitScaleY: pos.hitScaleY)
        default:
            EmptyView()
        }
    }
}

// MARK: - Clamp helper
private extension CGFloat {
    func clamped(_ lo: CGFloat, _ hi: CGFloat) -> CGFloat {
        Swift.min(Swift.max(self, lo), hi)
    }
}
