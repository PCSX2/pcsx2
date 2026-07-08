// LicenseView.swift — Third-party license display
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

struct LicenseEntry: Identifiable {
    let id = UUID()
    let name: String
    let license: String
    let copyright: String
}

private let licenses: [LicenseEntry] = [
    LicenseEntry(name: "PCSX2", license: "GPL v3", copyright: "© 2002-2025 PCSX2 Dev Team"),
    LicenseEntry(name: "pontos2024/PCSX2_ARM64", license: "GPL v3", copyright: "ARM64 recompiler port"),
    LicenseEntry(name: "ARMSX2", license: "GPL v3", copyright: "Android port"),
    LicenseEntry(name: "SDL3", license: "zlib License", copyright: "© Sam Lantinga"),
    LicenseEntry(name: "VIXL", license: "BSD 3-Clause", copyright: "© ARM Ltd."),
    LicenseEntry(name: "FFmpeg", license: "LGPL v2.1 / GPL v2", copyright: "© FFmpeg contributors"),
    LicenseEntry(name: "fmt", license: "MIT", copyright: "© Victor Zverovich"),
    LicenseEntry(name: "zlib", license: "zlib License", copyright: "© Jean-loup Gailly, Mark Adler"),
    LicenseEntry(name: "zstd", license: "BSD", copyright: "© Meta Platforms, Inc."),
    LicenseEntry(name: "lz4", license: "BSD 2-Clause", copyright: "© Yann Collet"),
    LicenseEntry(name: "LZMA SDK", license: "Public Domain", copyright: "Igor Pavlov"),
    LicenseEntry(name: "libzip", license: "BSD 3-Clause", copyright: "© Dieter Baron, Thomas Klausner"),
    LicenseEntry(name: "FreeType", license: "FreeType License / GPL v2", copyright: "© The FreeType Project"),
    LicenseEntry(name: "HarfBuzz", license: "MIT", copyright: "© HarfBuzz contributors"),
    LicenseEntry(name: "SoundTouch", license: "LGPL v2.1", copyright: "© Olli Parviainen"),
    LicenseEntry(name: "cubeb", license: "ISC", copyright: "© Mozilla Foundation"),
    LicenseEntry(name: "cpuinfo", license: "BSD 2-Clause", copyright: "© Facebook, Inc."),
    LicenseEntry(name: "libchdr", license: "BSD 3-Clause", copyright: "© libchdr contributors"),
    LicenseEntry(name: "glad", license: "MIT", copyright: "© David Herberth"),
    LicenseEntry(name: "glslang", license: "BSD 3-Clause / Apache 2.0", copyright: "© The Khronos Group Inc."),
    LicenseEntry(name: "plutosvg", license: "MIT", copyright: "© plutosvg contributors"),
    LicenseEntry(name: "rcheevos", license: "MIT", copyright: "© RetroAchievements contributors"),
    LicenseEntry(name: "discord-rpc", license: "MIT", copyright: "© Discord Inc."),
]

struct LicenseView: View {
    var body: some View {
        List(licenses) { entry in
            VStack(alignment: .leading, spacing: 4) {
                Text(entry.name)
                    .font(.headline)
                Text(entry.license)
                    .font(.subheadline)
                    .foregroundStyle(.blue)
                Text(entry.copyright)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            .padding(.vertical, 2)
        }
        .navigationTitle("Licenses")
        .navigationBarTitleDisplayMode(.inline)
    }
}
