// SettingsStore+UI.swift — UI/gamepad localization helpers
// SPDX-License-Identifier: GPL-3.0+

import Foundation
import SwiftUI

extension SettingsStore {
    func localized(_ key: String) -> String {
        appLanguage.localized(key)
    }

    var localizedLayoutDirection: LayoutDirection {
        appLanguage.layoutDirection
    }
}
