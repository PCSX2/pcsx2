// ARMSX2MacApp.swift
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

@main
struct ARMSX2MacApp: App {
    @StateObject private var settings: MacSettingsStore
    @StateObject private var gameLibrary: GameLibrary
    @StateObject private var biosLibrary: BIOSLibrary
    @StateObject private var launcher: EmulatorLauncher
    @StateObject private var quickMenu: ARMSX2QuickMenuController

    init() {
        let settings = MacSettingsStore()
        _settings = StateObject(wrappedValue: settings)
        _biosLibrary = StateObject(wrappedValue: BIOSLibrary(ini: settings.ini))
        _gameLibrary = StateObject(wrappedValue: GameLibrary())
        _launcher = StateObject(wrappedValue: EmulatorLauncher())
        _quickMenu = StateObject(wrappedValue: ARMSX2QuickMenuController())
    }

    var body: some Scene {
        WindowGroup {
            MacRootView()
                .environmentObject(settings)
                .environmentObject(gameLibrary)
                .environmentObject(biosLibrary)
                .environmentObject(launcher)
                .environmentObject(quickMenu)
                .frame(minWidth: 980, minHeight: 640)
        }
        .windowStyle(.titleBar)
        .commands {
            CommandGroup(after: .newItem) {
                Button("Import Games...") {
                    gameLibrary.importGames(FilePanels.chooseGames(), replaceExisting: true)
                }
                .keyboardShortcut("i", modifiers: [.command])

                Button("Import BIOS...") {
                    biosLibrary.importBIOS(FilePanels.chooseBIOS())
                }
                .keyboardShortcut("b", modifiers: [.command, .shift])
            }

            CommandMenu("Emulation") {
                Button("Choose Emulator Executable...") {
                    launcher.chooseExecutable()
                }

                Button("Start BIOS") {
                    launcher.bootBIOS(fullscreen: false)
                }
                .disabled(launcher.isRunning)

                Button("Stop Emulator") {
                    launcher.stop()
                }
                .disabled(!launcher.isRunning)
                .keyboardShortcut(".", modifiers: [.command])
            }

            CommandMenu("In Game") {
                Button("Open PCSX2 Pause Menu") {
                    launcher.sendHotkey(.openPauseMenu)
                }
                .disabled(!launcher.isRunning)
                .keyboardShortcut(.escape, modifiers: [])

                Button("Toggle OSD") {
                    settings.ensureUsableOSDPreset()
                    launcher.sendHotkey(.toggleOSD)
                }
                .disabled(!launcher.isRunning)
                .keyboardShortcut("o", modifiers: [.command, .shift])

                Button("Toggle Pause") {
                    launcher.sendHotkey(.togglePause)
                }
                .disabled(!launcher.isRunning)
                .keyboardShortcut("p", modifiers: [.command, .shift])

                Divider()

                Button("Save State") {
                    launcher.sendHotkey(.saveState)
                }
                .disabled(!launcher.isRunning)
                .keyboardShortcut("s", modifiers: [.command, .shift])

                Button("Load State") {
                    launcher.sendHotkey(.loadState)
                }
                .disabled(!launcher.isRunning)
                .keyboardShortcut("l", modifiers: [.command, .shift])

                Divider()

                Button("Toggle Frame Limit") {
                    launcher.sendHotkey(.toggleFrameLimit)
                }
                .disabled(!launcher.isRunning)

                Button("Toggle Software Renderer") {
                    launcher.sendHotkey(.toggleSoftwareRenderer)
                }
                .disabled(!launcher.isRunning)

                Button("Screenshot") {
                    launcher.sendHotkey(.screenshot)
                }
                .disabled(!launcher.isRunning)
            }
        }

        Settings {
            MacSettingsView()
                .environmentObject(settings)
                .environmentObject(gameLibrary)
                .environmentObject(launcher)
                .frame(width: 720, height: 560)
        }
    }
}
