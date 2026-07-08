// SettingsStore+Graphics.swift — graphics-domain config and helpers
// SPDX-License-Identifier: GPL-3.0+

import Foundation

extension SettingsStore {
    /// Homogeneous bool GS hacks, in display order.
    static let gsBoolHackOptions: [GameFixOption] = [
        .init(key: "paltex", label: "GPU Palette Conversion"),
        .init(key: "UserHacks_CPU_FB_Conversion", label: "CPU Framebuffer Conversion"),
        .init(key: "UserHacks_ReadTCOnClose", label: "Read Targets When Closing"),
        .init(key: "UserHacks_DisableDepthSupport", label: "Disable Depth Emulation"),
        .init(key: "UserHacks_DisablePartialInvalidation", label: "Disable Partial Invalidation"),
        .init(key: "preload_frame_with_gs_data", label: "Preload Frame Data"),
        .init(key: "UserHacks_EstimateTextureRegion", label: "Estimate Texture Region"),
        .init(key: "UserHacks_DrawBuffering", label: "Draw Buffering"),
        .init(key: "UserHacks_NativePaletteDraw", label: "Unscaled Palette Draw")
    ]

    static func aspectRatioName(for value: Int) -> String {
        switch value {
        case 0: return "Stretch"
        case 1: return "Auto 4:3/3:2"
        case 2: return "4:3"
        case 3: return "16:9"
        case 4: return "10:7"
        default: return "Auto 4:3/3:2"
        }
    }

    static func aspectRatioValue(from name: String) -> Int {
        switch name {
        case "Stretch", "0": return 0
        case "Auto 4:3/3:2", "1": return 1
        case "4:3", "2": return 2
        case "16:9", "3": return 3
        case "10:7", "4": return 4
        default: return 1
        }
    }

    static func supportedIOSRenderer(_ value: Int) -> Int {
        switch value {
        case 17, 13, 11:
            return value
        default:
            return 17
        }
    }
}
