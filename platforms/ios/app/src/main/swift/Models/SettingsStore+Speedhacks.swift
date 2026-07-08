// SettingsStore+Speedhacks.swift — speedhack/JIT boot-load helpers
// SPDX-License-Identifier: GPL-3.0+

import Foundation

extension SettingsStore {
    static func loadedFastBoot() -> Bool {
        let coreFastBoot = ARMSX2Bridge.getINIBool("EmuCore", key: "EnableFastBoot", defaultValue: false)
        return ARMSX2Bridge.getINIBool("GameISO", key: "FastBoot", defaultValue: coreFastBoot)
    }

    static func loadedJITScriptProtocol() -> JITScriptProtocol {
        let protocolValue = JITScriptProtocol.normalized(
            ARMSX2Bridge.getINIString("ARMSX2iOS/JIT", key: "ScriptProtocol", defaultValue: JITScriptProtocol.defaultValue.rawValue)
        )
        let migrated = ARMSX2Bridge.getINIBool("ARMSX2iOS/Migrations", key: "JITScriptProtocolByOSV1", defaultValue: false)
        if !migrated && JITScriptProtocol.defaultValue == .legacy && protocolValue == .universal {
            ARMSX2Bridge.setINIString("ARMSX2iOS/JIT", key: "ScriptProtocol", value: JITScriptProtocol.legacy.rawValue)
            ARMSX2Bridge.setINIBool("ARMSX2iOS/Migrations", key: "JITScriptProtocolByOSV1", value: true)
            NSLog("[ARMSX2 iOS Settings] Migrated JIT script protocol to legacy for this iOS version")
            return .legacy
        }
        if !migrated {
            ARMSX2Bridge.setINIBool("ARMSX2iOS/Migrations", key: "JITScriptProtocolByOSV1", value: true)
        }
        return protocolValue
    }
}
