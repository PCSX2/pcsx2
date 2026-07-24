// VirtualPadSettingsView.swift — Virtual pad opacity, haptic, layout editing
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI
import UIKit
import UniformTypeIdentifiers

private enum DynamicActionRole {
    case aim
    case fire
    case holdFire
}

struct VirtualPadSettingsView: View {
    @State private var settings = SettingsStore.shared
    @State private var dynamicSettings = DynamicThumbstickSettings.shared
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

            Section {
                Toggle(settings.localized("Invert Left Horizontal"), isOn: $settings.invertLeftStickX)
                Toggle(settings.localized("Invert Left Vertical"), isOn: $settings.invertLeftStickY)
                Toggle(settings.localized("Invert Right Horizontal"), isOn: $settings.invertRightStickX)
                Toggle(settings.localized("Invert Right Vertical (Camera)"), isOn: $settings.invertRightStickY)
            } header: {
                Text(settings.localized("Stick Inversion"))
            } footer: {
                Text(settings.localized("Flips the on-screen stick axes. Useful for games with fixed inverted camera or flight controls. Per-game overrides are available in the game’s Virtual Pad settings."))
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

            Section {
                ForEach(BuiltInDynamicControlPreset.allCases) { preset in
                    Button {
                        preset.apply(settings: dynamicSettings)
                    } label: {
                        HStack(spacing: 12) {
                            VStack(alignment: .leading, spacing: 4) {
                                Text(settings.localized(preset.rawValue))
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
                            if preset.isActive(settings: dynamicSettings) {
                                Image(systemName: "checkmark.circle.fill")
                                    .foregroundStyle(.green)
                                    .fixedSize()
                            }
                        }
                        .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                }
            } header: {
                Text(settings.localized("Dynamic Control Presets"))
            } footer: {
                Text(settings.localized("Selecting a preset changes only the listed Dynamic Control options. All sensitivity, button assignments, and other Virtual Pad settings are preserved."))
                    .lineLimit(nil)
                    .fixedSize(horizontal: false, vertical: true)
            }

            Section {
                Toggle(
                    settings.localized("Legacy Thumbsticks"),
                    isOn: Binding(
                        get: { dynamicSettings.legacyThumbsticks },
                        set: { dynamicSettings.setLegacyThumbsticks($0) }
                    )
                )
                Toggle(
                    settings.localized("Dynamic Thumbsticks"),
                    isOn: Binding(
                        get: { dynamicSettings.dynamicThumbsticks },
                        set: { dynamicSettings.setDynamicThumbsticks($0) }
                    )
                )
                Toggle(settings.localized("Swipe Camera"), isOn: $dynamicSettings.swipeCamera)
                Toggle(settings.localized("Gyroscope Camera"), isOn: $dynamicSettings.gyroscopeCamera)
            } header: {
                Text(settings.localized("Dynamic Controls"))
            } footer: {
                Text(settings.localized("Legacy and Dynamic Thumbsticks are mutually exclusive. Dynamic sticks appear where each touch begins. Swipe Camera replaces the right touch stick, while Gyroscope Camera augments the active right-side control."))
            }

            controlSensitivitySection

            if dynamicSettings.dynamicThumbsticks {
                thumbstickActionButtonsSection(
                    title: settings.localized("Action Buttons Left Thumbstick"),
                    toggleTitle: settings.localized("Dynamic Actions in Left Thumbstick"),
                    isEnabled: $dynamicSettings.leftThumbstickActionsEnabled,
                    aim: $dynamicSettings.leftAimButton,
                    fire: $dynamicSettings.leftFireButton,
                    holdFire: $dynamicSettings.leftHoldFireButton
                )
            }

            if dynamicSettings.swipeCamera || dynamicSettings.dynamicThumbsticks {
                thumbstickActionButtonsSection(
                    title: settings.localized("Action Buttons Right Thumbstick"),
                    toggleTitle: settings.localized("Dynamic Actions in Right Thumbstick"),
                    isEnabled: $dynamicSettings.rightThumbstickActionsEnabled,
                    aim: $dynamicSettings.rightAimButton,
                    fire: $dynamicSettings.rightFireButton,
                    holdFire: $dynamicSettings.rightHoldFireButton
                )
            }

            Section {
                Toggle(
                    settings.localized("Dynamic Aiming Crosshair"),
                    isOn: $dynamicSettings.dynamicCrosshairEnabled
                )
                if dynamicSettings.dynamicCrosshairEnabled {
                    DynamicControlSlider(
                        title: settings.localized("Crosshair Size"),
                        value: $dynamicSettings.dynamicCrosshairSize,
                        range: 12...120,
                        step: 1,
                        valueLabel: { "\(Int($0)) pt" }
                    )
                    DynamicControlSlider(
                        title: settings.localized("Crosshair Opacity"),
                        value: $dynamicSettings.dynamicCrosshairOpacity,
                        range: 0.10...1,
                        step: 0.05,
                        valueLabel: percentageLabel
                    )
                    Picker(
                        settings.localized("Crosshair Type"),
                        selection: $dynamicSettings.dynamicCrosshairType
                    ) {
                        ForEach(DynamicCrosshairType.allCases) { type in
                            Text(settings.localized(type.title)).tag(type)
                        }
                    }
                    Picker(
                        settings.localized("Crosshair Animation"),
                        selection: $dynamicSettings.dynamicCrosshairAnimation
                    ) {
                        ForEach(DynamicCrosshairAnimation.allCases) { animation in
                            Text(settings.localized(animation.title)).tag(animation)
                        }
                    }
                }
            } header: {
                Text(settings.localized("Dynamic Crosshair"))
            } footer: {
                Text(settings.localized("The crosshair appears only while Aim Mode is active. Every animation follows live swipe, thumbstick, and gyroscope direction and speed, then reacts separately to single shots and automatic fire."))
            }

            if dynamicSettings.dynamicThumbsticks {
                Section {
                    DynamicControlSlider(
                        title: settings.localized("Maximum Radius"),
                        value: $dynamicSettings.thumbstickRadius,
                        range: 40...60,
                        step: 1,
                        valueLabel: { "\(Int($0)) pt" }
                    )
                    DynamicControlSlider(
                        title: settings.localized("Dead Zone"),
                        value: $dynamicSettings.deadZone,
                        range: 0...0.25,
                        step: 0.01,
                        valueLabel: percentageLabel
                    )
                    DynamicControlSlider(
                        title: settings.localized("Thumbstick Opacity"),
                        value: $dynamicSettings.thumbstickOpacity,
                        range: 0...1,
                        step: 0.01,
                        valueLabel: percentageLabel
                    )
                    DynamicControlSlider(
                        title: settings.localized("Base Opacity"),
                        value: $dynamicSettings.baseOpacity,
                        range: 0...1,
                        step: 0.01,
                        valueLabel: percentageLabel
                    )
                    DynamicControlSlider(
                        title: settings.localized("Trail Opacity"),
                        value: $dynamicSettings.trailOpacity,
                        range: 0...1,
                        step: 0.01,
                        valueLabel: percentageLabel
                    )
                    Toggle(settings.localized("Activation Haptics"), isOn: $dynamicSettings.activationHaptics)
                } header: {
                    Text(settings.localized("Dynamic Thumbstick Feel"))
                } footer: {
                    Text(settings.localized("The compact base stays at the initial touch point. Dead zone begins at 0% and progressively reaches the selected value as the stick moves outward. Analog output saturates at the selected radius while the nub and seven-dot trail continue following overdrag."))
                }
            }

            if dynamicSettings.gyroscopeCamera {
                Section {
                    DynamicControlSlider(
                        title: settings.localized("Gyro Sensitivity"),
                        value: $dynamicSettings.gyroSensitivity,
                        range: 0.5...4.0,
                        step: 0.1,
                        valueLabel: { String(format: "%.1fx", $0) }
                    )
                    DynamicControlSlider(
                        title: settings.localized("Gyro Acceleration"),
                        value: $dynamicSettings.gyroAcceleration,
                        range: 0...2,
                        step: 0.05,
                        valueLabel: percentageLabel
                    )
                    DynamicControlSlider(
                        title: settings.localized("Gyro Smoothing"),
                        value: $dynamicSettings.gyroSmoothing,
                        range: 0...0.95,
                        step: 0.05,
                        valueLabel: percentageLabel
                    )
                    DynamicControlSlider(
                        title: settings.localized("Gyro Dead Zone"),
                        value: $dynamicSettings.gyroDeadZone,
                        range: 0...0.25,
                        step: 0.01,
                        valueLabel: { String(format: "%.2f rad/s", $0) }
                    )
                    DynamicControlSlider(
                        title: settings.localized("Maximum Gyro Rate"),
                        value: $dynamicSettings.gyroMaximumRate,
                        range: 1...12,
                        step: 0.5,
                        valueLabel: { String(format: "%.1f rad/s", $0) }
                    )
                    Toggle(settings.localized("Invert Gyro Horizontal"), isOn: $dynamicSettings.invertGyroHorizontal)
                    Toggle(settings.localized("Invert Gyro Vertical"), isOn: $dynamicSettings.invertGyroVertical)
                } header: {
                    Text(settings.localized("Gyroscope"))
                } footer: {
                    Text(settings.localized("Gyroscope input is active only while the virtual controller is on screen. If the sensor is unavailable, the selected touch camera continues working normally."))
                }
            }

            if dynamicSettings.swipeCamera ||
                (dynamicSettings.dynamicThumbsticks &&
                    (dynamicSettings.leftThumbstickActionsEnabled || dynamicSettings.rightThumbstickActionsEnabled)) {
                Section {
                    Toggle(
                        dynamicActionTitle("Hold Aim While Touching Camera", role: .aim),
                        isOn: Binding(
                            get: { dynamicSettings.holdAimWhileSwipe },
                            set: { dynamicSettings.setHoldAimWhileSwipe($0) }
                        )
                    )
                    Toggle(
                        dynamicActionTitle("Double Tap to Hold Aim", role: .aim),
                        isOn: Binding(
                            get: { dynamicSettings.doubleTapToHoldAim },
                            set: { dynamicSettings.setDoubleTapToHoldAim($0) }
                        )
                    )
                    DynamicControlSlider(
                        title: dynamicActionTitle("Aim Release Delay", role: .aim),
                        value: $dynamicSettings.aimReleaseDelay,
                        range: 0...2,
                        step: 0.05,
                        valueLabel: durationLabel
                    )
                    .disabled(!dynamicSettings.holdAimWhileSwipe && !dynamicSettings.doubleTapToHoldAim)
                    DynamicControlSlider(
                        title: dynamicActionTitle("Double-Tap Window", role: .aim),
                        value: $dynamicSettings.doubleTapWindow,
                        range: 0.15...0.60,
                        step: 0.01,
                        valueLabel: durationLabel
                    )
                    .disabled(!dynamicSettings.doubleTapToHoldAim)
                    Toggle(
                        dynamicActionTitle("Tap to Fire Single Shots", role: .fire),
                        isOn: $dynamicSettings.tapToFire
                    )
                    DynamicControlSlider(
                        title: dynamicActionTitle("Single-Shot Tap Duration", role: .fire),
                        value: $dynamicSettings.tapMaximumDuration,
                        range: 0.10...0.60,
                        step: 0.01,
                        valueLabel: durationLabel
                    )
                    DynamicControlSlider(
                        title: dynamicActionTitle("Single-Shot Travel Tolerance", role: .fire),
                        value: $dynamicSettings.tapTravelTolerance,
                        range: 4...30,
                        step: 1,
                        valueLabel: { "\(Int($0)) pt" }
                    )
                    Toggle(
                        dynamicActionTitle("Multiple Taps Enable Automatic Fire", role: .holdFire),
                        isOn: $dynamicSettings.rapidTapFireEnabled
                    )
                    DynamicControlSlider(
                        title: dynamicActionTitle("Multiple-Tap Window", role: .holdFire),
                        value: $dynamicSettings.rapidTapWindow,
                        range: 0.10...0.80,
                        step: 0.01,
                        valueLabel: durationLabel
                    )
                    .disabled(!dynamicSettings.rapidTapFireEnabled)
                    DynamicControlSlider(
                        title: dynamicActionTitle("Taps to Activate", role: .holdFire),
                        value: Binding(
                            get: { Double(dynamicSettings.rapidTapActivationCount) },
                            set: { dynamicSettings.rapidTapActivationCount = Int($0.rounded()) }
                        ),
                        range: 2...5,
                        step: 1,
                        valueLabel: { "\(Int($0)) taps" }
                    )
                    .disabled(!dynamicSettings.rapidTapFireEnabled)
                    DynamicControlSlider(
                        title: dynamicActionTitle("Automatic Fire Interval", role: .holdFire),
                        value: $dynamicSettings.automaticFireInterval,
                        range: 0.06...0.50,
                        step: 0.01,
                        valueLabel: durationLabel
                    )
                    .disabled(!dynamicSettings.rapidTapFireEnabled)
                    Toggle(
                        dynamicActionTitle("Extend Automatic Fire While Dragging", role: .holdFire),
                        isOn: $dynamicSettings.extendFireWhileDragging
                    )
                        .disabled(!dynamicSettings.rapidTapFireEnabled)
                    Toggle(
                        dynamicActionTitle("Release Fire When Touch Ends", role: .holdFire),
                        isOn: $dynamicSettings.releaseFireWhenTouchEnds
                    )
                        .disabled(!dynamicSettings.rapidTapFireEnabled)
                    DynamicControlSlider(
                        title: dynamicActionTitle("Fire Release Delay", role: .holdFire),
                        value: $dynamicSettings.fireReleaseDelay,
                        range: 0...1,
                        step: 0.05,
                        valueLabel: durationLabel
                    )
                    .disabled(!dynamicSettings.rapidTapFireEnabled || dynamicSettings.releaseFireWhenTouchEnds)
                } header: {
                    Text(settings.localized("Dynamic Actions"))
                }
            }

            Section {
                Button(settings.localized("Restore Dynamic Control Defaults"), role: .destructive) {
                    dynamicSettings.restoreDefaults()
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

    @ViewBuilder
    private var controlSensitivitySection: some View {
        Section {
            DynamicControlSlider(
                title: settings.localized("Movement Sensitivity"),
                value: $dynamicSettings.movementSensitivity,
                range: 0.33...2.0,
                step: 0.01,
                valueLabel: percentageLabel
            )
            DynamicControlSlider(
                title: settings.localized("Look Sensitivity"),
                value: $dynamicSettings.lookSensitivity,
                range: 0.43...1.71,
                step: 0.01,
                valueLabel: percentageLabel
            )
            DynamicSwipeSensitivityControl(
                title: settings.localized("Swipe Sensitivity"),
                showsEnableToggle: false,
                isEnabled: .constant(true),
                value: $dynamicSettings.swipeSensitivity,
                horizontalSensitivity: $dynamicSettings.swipeHorizontalSensitivity,
                verticalSensitivity: $dynamicSettings.swipeVerticalSensitivity
            )
            .disabled(!dynamicSettings.swipeCamera)
            DynamicSwipeSensitivityControl(
                title: settings.localized("Sensitivity While on Aim Mode"),
                showsEnableToggle: true,
                isEnabled: $dynamicSettings.swipeSensitivityWhileAimingEnabled,
                value: $dynamicSettings.swipeSensitivityWhileAiming,
                horizontalSensitivity: $dynamicSettings.swipeHorizontalSensitivityWhileAiming,
                verticalSensitivity: $dynamicSettings.swipeVerticalSensitivityWhileAiming
            )
            .disabled(!dynamicSettings.swipeCamera)
            DynamicSwipeSensitivityControl(
                title: settings.localized("Sensitivity While Not Aiming"),
                showsEnableToggle: true,
                isEnabled: $dynamicSettings.swipeSensitivityWhileNotAimingEnabled,
                value: $dynamicSettings.swipeSensitivityWhileNotAiming,
                horizontalSensitivity: $dynamicSettings.swipeHorizontalSensitivityWhileNotAiming,
                verticalSensitivity: $dynamicSettings.swipeVerticalSensitivityWhileNotAiming
            )
            .disabled(!dynamicSettings.swipeCamera)
        } header: {
            Text(settings.localized("Dynamic Control Sensitivity"))
        } footer: {
            Text(settings.localized("Each swipe profile has overall, horizontal, and vertical sensitivity. Disabled aim-state profiles fall back to Swipe Sensitivity."))
        }
    }

    @ViewBuilder
    private func thumbstickActionButtonsSection(
        title: String,
        toggleTitle: String,
        isEnabled: Binding<Bool>,
        aim: Binding<VirtualPadActionButton>,
        fire: Binding<VirtualPadActionButton>,
        holdFire: Binding<VirtualPadActionButton>
    ) -> some View {
        Section {
            Toggle(toggleTitle, isOn: isEnabled)
            if isEnabled.wrappedValue {
                Picker(settings.localized("Aim (Hold Thumbstick)"), selection: aim) {
                    ForEach(VirtualPadActionButton.allCases) { button in
                        Text(settings.localized(button.title)).tag(button)
                    }
                }
                Picker(settings.localized("Fire (Tap Thumbstick)"), selection: fire) {
                    ForEach(VirtualPadActionButton.allCases) { button in
                        Text(settings.localized(button.title)).tag(button)
                    }
                }
                Picker(settings.localized("Hold Fire (Fast Tap Thumbstick)"), selection: holdFire) {
                    ForEach(VirtualPadActionButton.allCases) { button in
                        Text(settings.localized(button.title)).tag(button)
                    }
                }
            }
        } header: {
            Text(title)
        }
    }

    private func dynamicActionTitle(_ baseTitle: String, role: DynamicActionRole) -> String {
        var configuredButtons: [VirtualPadActionButton] = []
        if dynamicSettings.rightThumbstickActionsEnabled {
            configuredButtons.append(dynamicActionButton(role: role, side: .right))
        }
        if dynamicSettings.leftThumbstickActionsEnabled {
            configuredButtons.append(dynamicActionButton(role: role, side: .left))
        }
        if configuredButtons.isEmpty {
            configuredButtons.append(dynamicActionButton(role: role, side: .right))
        }

        var seen: Set<VirtualPadActionButton> = []
        let buttonNames = configuredButtons.compactMap { button -> String? in
            guard seen.insert(button).inserted else { return nil }
            return settings.localized(button.title)
        }
        return "\(settings.localized(baseTitle)) (\(buttonNames.joined(separator: " / ")))"
    }

    private func dynamicActionButton(
        role: DynamicActionRole,
        side: VirtualPadThumbstickSide
    ) -> VirtualPadActionButton {
        switch role {
        case .aim: return dynamicSettings.aimButton(for: side)
        case .fire: return dynamicSettings.fireButton(for: side)
        case .holdFire: return dynamicSettings.holdFireButton(for: side)
        }
    }

    private func percentageLabel(_ value: Double) -> String {
        "\(Int((value * 100).rounded()))%"
    }

    private func durationLabel(_ value: Double) -> String {
        String(format: "%.2f s", value)
    }
}

private struct DynamicControlSlider: View {
    let title: String
    @Binding var value: Double
    let range: ClosedRange<Double>
    let step: Double
    let valueLabel: (Double) -> String

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text(title)
                Spacer()
                Text(valueLabel(value))
                    .foregroundStyle(.secondary)
                    .monospacedDigit()
            }
            Slider(value: $value, in: range, step: step)
        }
        .accessibilityElement(children: .combine)
    }
}

private struct DynamicSwipeSensitivityControl: View {
    let title: String
    let showsEnableToggle: Bool
    @Binding var isEnabled: Bool
    @Binding var value: Double
    @Binding var horizontalSensitivity: Double
    @Binding var verticalSensitivity: Double

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if showsEnableToggle {
                Toggle(isOn: $isEnabled) {
                    sensitivityLabel
                }
            } else {
                sensitivityLabel
            }
            Slider(value: $value, in: 0.08...0.75, step: 0.01)
                .disabled(!isEnabled)
                .accessibilityLabel(title)
            DynamicControlSlider(
                title: "Horizontal Swipe Sensitivity",
                value: $horizontalSensitivity,
                range: 0.25...2,
                step: 0.01,
                valueLabel: percentageLabel
            )
            .disabled(!isEnabled)
            DynamicControlSlider(
                title: "Vertical Swipe Sensitivity",
                value: $verticalSensitivity,
                range: 0.25...2,
                step: 0.01,
                valueLabel: percentageLabel
            )
            .disabled(!isEnabled)
        }
    }

    private var sensitivityLabel: some View {
        HStack {
            Text(title)
            Spacer()
            Text(String(format: "%.2f°/pt", value))
                .foregroundStyle(.secondary)
                .monospacedDigit()
        }
    }

    private func percentageLabel(_ value: Double) -> String {
        "\(Int((value * 100).rounded()))%"
    }
}
