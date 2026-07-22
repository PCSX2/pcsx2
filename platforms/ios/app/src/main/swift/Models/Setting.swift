// Setting.swift — configuration-only property wrapper for INI-backed settings
// SPDX-License-Identifier: GPL-3.0+

import Foundation

/// Configuration-only: holds the INI section/key/writer for a setting. The
/// @Observable macro owns the stored property; didSet consults this config.
///
/// `onSet` runs from each property's `didSet`. Swift suppresses property
/// observers during `init()`, so `onSet` is not reached while
/// `SettingsStore.init()` is running; the graphics-pipeline closures still go
/// through `requestGraphicsApplyGuarded()` (which no-ops while INI is loading)
/// as a guard, should that ever change.
struct Setting<Value> {
    let section: String
    let key: String
    let defaultValue: Value
    let suppressible: Bool
    let writer: (String, String, Value) -> Void
    let onSet: ((Value) -> Void)?

    init(section: String,
         key: String,
         default defaultValue: Value,
         suppressible: Bool = true,
         writer: @escaping (String, String, Value) -> Void,
         onSet: ((Value) -> Void)? = nil) {
        self.section = section
        self.key = key
        self.defaultValue = defaultValue
        self.suppressible = suppressible
        self.writer = writer
        self.onSet = onSet
    }
}
