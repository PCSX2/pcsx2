// EmulatorView.swift — Metal rendering surface placeholder
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct EmulatorView: View {
    var body: some View {
        // The Metal rendering surface is owned by SDL3 and composited
        // behind this SwiftUI overlay. This view is transparent to let
        // the SDL3 Metal layer show through.
        Color.clear
    }
}
