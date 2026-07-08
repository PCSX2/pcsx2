// PerGameTab.swift — Container for a per-game settings tab.
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct PerGameTab<Content: View>: View {
    let title: String
    @ViewBuilder let content: Content

    var body: some View {
        Form {
            content
        }
        .scrollContentBackground(.hidden)
        .navigationTitle(title)
        .navigationBarTitleDisplayMode(.inline)
        .toolbarBackground(OverlayTheme.shell, for: .navigationBar)
        .toolbarBackground(.visible, for: .navigationBar)
    }
}
