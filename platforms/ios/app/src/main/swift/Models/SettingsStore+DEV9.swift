// SettingsStore+DEV9.swift — DEV9/network normalization helpers
// SPDX-License-Identifier: GPL-3.0+

import Foundation

extension SettingsStore {
    /// Marks the DEV9 HDD image file as excluded from iCloud/iTunes backup so a
    /// multi-gigabyte image does not fill the user's backup. Targets only the
    /// image file (not the inis directory, which also holds small config files
    /// worth backing up). Called when HDD is enabled and on settings reload so
    /// the flag is applied once the core has created the image.
    func excludeHddImageFromBackup() {
        let documents = ARMSX2Bridge.documentsDirectory()
        let fileName = dev9HddFile.isEmpty ? "DEV9hdd.raw" : dev9HddFile
        var imageURL = URL(fileURLWithPath: (documents as NSString)
            .appendingPathComponent("iPSX2/inis"))
            .appendingPathComponent(fileName)
        guard FileManager.default.fileExists(atPath: imageURL.path) else { return }
        var values = URLResourceValues()
        values.isExcludedFromBackup = true
        try? imageURL.setResourceValues(values)
    }

    func normalizeDEV9Settings() {
        if dev9HddEnabled {
            ARMSX2Bridge.setINIString("DEV9/Hdd", key: "HddFile", value: dev9HddFile.isEmpty ? "DEV9hdd.raw" : dev9HddFile)
        }

        if dev9EthernetEnabled {
            ARMSX2Bridge.setINIString("DEV9/Eth", key: "EthApi", value: "Sockets")
            ARMSX2Bridge.setINIString("DEV9/Eth", key: "EthDevice", value: dev9EthDevice.isEmpty ? "Auto" : dev9EthDevice)
        }
    }
}
