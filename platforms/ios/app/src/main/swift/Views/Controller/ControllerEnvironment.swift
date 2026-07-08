// ControllerEnvironment.swift — pad environment keys shared across the controller views
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

private struct PadOpacityKey: EnvironmentKey {
    static let defaultValue: Double = 1.0
}

private struct PadSkinKey: EnvironmentKey {
    static let defaultValue: VirtualPadSkin = .armsx2Refresh
}

private struct PadSkinDescriptorKey: EnvironmentKey {
    static let defaultValue: VPadSkinDescriptor = VPadSkinLibraryStore.defaultDescriptor
}

private struct PadUsesFullSkinKey: EnvironmentKey {
    static let defaultValue = false
}

extension EnvironmentValues {
    var padOpacity: Double {
        get { self[PadOpacityKey.self] }
        set { self[PadOpacityKey.self] = newValue }
    }

    var padSkin: VirtualPadSkin {
        get { self[PadSkinKey.self] }
        set { self[PadSkinKey.self] = newValue }
    }

    var padSkinDescriptor: VPadSkinDescriptor {
        get { self[PadSkinDescriptorKey.self] }
        set { self[PadSkinDescriptorKey.self] = newValue }
    }

    var padUsesFullSkin: Bool {
        get { self[PadUsesFullSkinKey.self] }
        set { self[PadUsesFullSkinKey.self] = newValue }
    }
}
