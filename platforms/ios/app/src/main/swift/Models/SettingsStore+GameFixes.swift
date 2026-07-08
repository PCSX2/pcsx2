// SettingsStore+GameFixes.swift — manual game-fix config
// SPDX-License-Identifier: GPL-3.0+

import Foundation

extension SettingsStore {
    /// Manual EmuCore/Gamefixes toggles, in display order.
    static let gameFixOptions: [GameFixOption] = [
        .init(key: "VuAddSubHack", label: "VU Add-Sub Hack"),
        .init(key: "FpuMulHack", label: "FPU Multiply Hack"),
        .init(key: "XgKickHack", label: "Extra XGKICK Hack"),
        .init(key: "EETimingHack", label: "EE Timing Hack"),
        .init(key: "InstantDMAHack", label: "Instant DMA Hack"),
        .init(key: "SoftwareRendererFMVHack", label: "Software Renderer FMV Hack"),
        .init(key: "SkipMPEGHack", label: "Skip MPEG Hack"),
        .init(key: "OPHFlagHack", label: "OPH Flag Hack"),
        .init(key: "DMABusyHack", label: "DMA Busy Hack"),
        .init(key: "VIF1StallHack", label: "VIF1 Stall Hack"),
        .init(key: "GIFFIFOHack", label: "GIF FIFO Hack"),
        .init(key: "GoemonTlbHack", label: "Goemon TLB Hack"),
        .init(key: "IbitHack", label: "I-Bit Hack"),
        .init(key: "VUSyncHack", label: "VU Sync Hack"),
        .init(key: "VUOverflowHack", label: "VU Overflow Hack"),
        .init(key: "BlitInternalFPSHack", label: "Blit Internal FPS Hack"),
        .init(key: "FullVU0SyncHack", label: "Full VU0 Sync Hack")
    ]
}
