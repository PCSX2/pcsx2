// PadTab.swift — Per-game Virtual Pad category tab.
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct PadTab: View {
    @Binding var padLayoutIdentity: PadLayoutGameIdentity?
    @Binding var showPadLayoutEditor: Bool

    let layoutPresets: PadLayoutPresetStore
    let skinLibrary: VPadSkinLibraryStore

    var body: some View {
        PerGameTab(title: "Virtual Pad") {
            Section("Virtual Pad") {
                if let padLayoutIdentity {
                    Picker("Layout", selection: Binding<String?>(
                        get: { layoutPresets.presetID(for: padLayoutIdentity) },
                        set: { layoutPresets.setPreset($0, for: padLayoutIdentity) }
                    )) {
                        Text("Global Default (\(globalLayoutDisplayName))").tag(nil as String?)
                        ForEach(layoutPresets.presets) { preset in
                            Text(preset.displayName).tag(Optional(preset.id))
                        }
                    }

                    Picker("Skin", selection: Binding<String?>(
                        get: { validPerGameSkinID(for: padLayoutIdentity) },
                        set: { skinID in
                            if let skinID {
                                layoutPresets.setSkin(skinID, for: padLayoutIdentity, using: skinLibrary)
                            } else {
                                layoutPresets.clearSkin(for: padLayoutIdentity)
                            }
                        }
                    )) {
                        Text("Global Default (\(globalSkinDisplayName))").tag(nil as String?)
                        ForEach(skinLibrary.allDescriptors) { skin in
                            Text(skin.displayName).tag(Optional(skin.id))
                        }
                    }

                    if let linkedLayoutID = linkedLayoutIDForCurrentSkin,
                       let linkedLayout = layoutPresets.preset(id: linkedLayoutID) {
                        Button {
                            layoutPresets.setPreset(linkedLayoutID, for: padLayoutIdentity)
                        } label: {
                            Label("Apply Linked Skin Layout to This Game", systemImage: "square.and.arrow.down")
                        }
                        Text("Applies \(linkedLayout.displayName) for this game only. The selected skin is unchanged.")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }

                    Button {
                        showPadLayoutEditor = true
                    } label: {
                        Label("Edit Layout for This Game", systemImage: "square.resize")
                    }

                    Button("Reset VPad Layout to Global") {
                        layoutPresets.setPreset(nil, for: padLayoutIdentity)
                    }

                    Button("Reset VPad Skin to Global") {
                        layoutPresets.clearSkin(for: padLayoutIdentity)
                    }

                    Button(role: .destructive) {
                        layoutPresets.clearVPadOverrides(for: padLayoutIdentity)
                    } label: {
                        Label("Reset All VPad Overrides", systemImage: "arrow.counterclockwise")
                    }
                } else {
                    Text("Start this game once before choosing a custom layout or skin.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        }
    }

    private var globalLayoutDisplayName: String {
        layoutPresets.effectivePreset(for: nil)?.displayName ?? "Current Layout"
    }

    private var globalSkinDisplayName: String {
        skinLibrary.selectedDescriptor.displayName
    }

    private var linkedLayoutIDForCurrentSkin: String? {
        guard let descriptor = currentPerGameSkinDescriptor,
              let linkedLayoutID = descriptor.linkedLayoutPresetID,
              layoutPresets.preset(id: linkedLayoutID) != nil else {
            return nil
        }
        return linkedLayoutID
    }

    private var currentPerGameSkinDescriptor: VPadSkinDescriptor? {
        layoutPresets.effectiveSkinDescriptor(for: padLayoutIdentity, using: skinLibrary)
    }

    private func validPerGameSkinID(for identity: PadLayoutGameIdentity) -> String? {
        guard let skinID = layoutPresets.skinID(for: identity),
              skinLibrary.descriptor(id: skinID) != nil else {
            return nil
        }
        return skinID
    }
}
