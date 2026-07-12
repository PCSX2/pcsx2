// RetroAchievementsSettingsView.swift — RetroAchievements account and status UI
// SPDX-License-Identifier: GPL-3.0+

import SwiftUI

private let retroAchievementsNotification = Notification.Name("ARMSX2RetroAchievementsStateChanged")

struct RetroAchievementsSettingsView: View {
    @State private var settings = SettingsStore.shared
    @State private var state: [String: Any] = [:]
    @State private var achievementsEnabled = false
    @State private var hardcoreEnabled = false
    @State private var notificationsEnabled = true
    @State private var leaderboardNotificationsEnabled = true
    @State private var overlaysEnabled = true
    @State private var username = ""
    @State private var password = ""
    @State private var showingLogin = false
    @State private var loggingIn = false
    @State private var messageTitle = ""
    @State private var messageBody = ""
    @State private var showingMessage = false
    @State private var showingHardcoreDisableConfirm = false

    private func applyHardcoreChange(_ enabled: Bool) {
        hardcoreEnabled = enabled
        ARMSX2Bridge.setRetroAchievementsHardcore(enabled)
        if enabled && bool("hasActiveGame") && !bool("hardcoreActive") {
            showMessage(
                title: "Hardcore Mode",
                body: settings.localized("Hardcore mode will apply after a full reset of the current game.")
            )
        } else if enabled && !bool("hasActiveGame") {
            showMessage(
                title: "Hardcore Mode",
                body: settings.localized("Hardcore mode is ready and will apply when you boot a game.")
            )
        }
        refreshSoon()
    }

    var body: some View {
        Form {
            Section {
                Toggle(settings.localized("Enable RetroAchievements"), isOn: Binding(
                    get: { achievementsEnabled },
                    set: { newValue in
                        guard achievementsSupported || !newValue else {
                            achievementsEnabled = false
                            ARMSX2Bridge.setRetroAchievementsEnabled(false)
                            showMessage(title: "RetroAchievements", body: unavailableMessage)
                            refreshSoon()
                            return
                        }
                        achievementsEnabled = newValue
                        ARMSX2Bridge.setRetroAchievementsEnabled(newValue)
                        refreshSoon()
                    }
                ))
                .disabled(!achievementsSupported)

                statusRow("Client", value: bool("active") ? "Active" : "Inactive")
                statusRow("Account", value: accountSummary, localizeValue: !hasStoredAccount)
            } header: {
                Text(settings.localized("RetroAchievements"))
            } footer: {
                Text(settings.localized(achievementsSupported ?
                    "Uses the same achievements core as ARMSX2 Android. Login tokens are stored in ARMSX2's local config; passwords are not stored by this screen." :
                    unavailableMessage))
            }

            if achievementsSupported {
                Section(settings.localized("Account")) {
                    if hasStoredAccount {
                        statusRow("User", value: displayName, localizeValue: false)
                        if bool("loggedIn") {
                            statusRow("Points", value: hardcoreSupported ?
                                "\(int("points")) hard / \(int("softcorePoints")) soft" :
                                "\(int("softcorePoints"))", localizeValue: false)
                            if int("unreadMessages") > 0 {
                                statusRow("Messages", value: "\(int("unreadMessages")) \(settings.localized("unread"))", localizeValue: false)
                            }
                        } else {
                            // Stored account but not logged in (token expired or session dead).
                            // Show a re-login button so the user is not stuck behind "Log Out".
                            statusRow("Session", value: settings.localized("Not connected — log in again"))
                        }

                        Button(role: .destructive) {
                            ARMSX2Bridge.logoutRetroAchievements()
                            refreshSoon()
                        } label: {
                            Text(settings.localized("Log Out"))
                        }

                        if !bool("loggedIn") {
                            Button {
                                username = string("username")
                                password = ""
                                showingLogin = true
                            } label: {
                                Text(settings.localized(loggingIn ? "Logging In..." : "Log In Again"))
                            }
                            .disabled(!achievementsEnabled || loggingIn)
                        }
                    } else {
                        Button {
                            username = string("username")
                            password = ""
                            showingLogin = true
                        } label: {
                            Text(settings.localized(loggingIn ? "Logging In..." : "Log In"))
                        }
                        .disabled(!achievementsEnabled || loggingIn)
                    }
                }

                Section {
                    if hardcoreSupported {
                        Toggle(settings.localized("Hardcore Mode"), isOn: Binding(
                            get: { hardcoreEnabled },
                            set: { newValue in
                                if newValue {
                                    applyHardcoreChange(true)
                                } else {
                                    showingHardcoreDisableConfirm = true
                                }
                            }
                        ))
                        .disabled(!achievementsEnabled)
                        .confirmationDialog(
                            settings.localized("Turn off Hardcore Mode?"),
                            isPresented: $showingHardcoreDisableConfirm,
                            titleVisibility: .visible
                        ) {
                            Button(settings.localized("Turn Off Hardcore"), role: .destructive) {
                                applyHardcoreChange(false)
                            }
                            Button(settings.localized("Cancel"), role: .cancel) {}
                        } message: {
                            Text(settings.localized("Disabling Hardcore drops you to Casual mode for the rest of this session. Re-enable it after resetting the game."))
                        }

                        statusRow("Hardcore Status", value: hardcoreStatus)
                    }

                    Toggle(settings.localized("Achievement Notifications"), isOn: Binding(
                        get: { notificationsEnabled },
                        set: { newValue in
                            notificationsEnabled = newValue
                            ARMSX2Bridge.setRetroAchievementsNotifications(newValue)
                            refreshSoon()
                        }
                    ))
                    .disabled(!achievementsEnabled)

                    Toggle(settings.localized("Leaderboard Notifications"), isOn: Binding(
                        get: { leaderboardNotificationsEnabled },
                        set: { newValue in
                            leaderboardNotificationsEnabled = newValue
                            ARMSX2Bridge.setRetroAchievementsLeaderboards(newValue)
                            refreshSoon()
                        }
                    ))
                    .disabled(!achievementsEnabled)

                    Toggle(settings.localized("In-Game Overlays"), isOn: Binding(
                        get: { overlaysEnabled },
                        set: { newValue in
                            overlaysEnabled = newValue
                            ARMSX2Bridge.setRetroAchievementsOverlays(newValue)
                            refreshSoon()
                        }
                    ))
                    .disabled(!achievementsEnabled)
                } header: {
                    Text(settings.localized("Modes"))
                } footer: {
                    if hardcoreSupported {
                        Text(settings.localized("Hardcore blocks cheat engines, PNACH cheat imports, slowdown, frame advance, and save-state loading. Turning Hardcore on during a running game applies after a full reset."))
                    }
                }

                Section(settings.localized("Current Game")) {
                    if bool("hasActiveGame") {
                        statusRow("Title", value: string("gameTitle", fallback: settings.localized("Unknown Game")), localizeValue: false)
                        statusRow("Game ID", value: "\(int("gameId"))", localizeValue: false)

                        if int("totalAchievements") > 0 {
                            statusRow("Achievements", value: "\(int("unlockedAchievements")) / \(int("totalAchievements"))", localizeValue: false)
                            statusRow("Points", value: "\(int("unlockedPoints")) / \(int("totalPoints"))", localizeValue: false)
                        } else if bool("hasAchievements") {
                            statusRow("Achievements", value: "Loaded")
                        } else {
                            statusRow("Achievements", value: "None found")
                        }

                        if bool("hasLeaderboards") {
                            statusRow("Leaderboards", value: "Available")
                        }

                        if bool("hasRichPresence") {
                            statusRow("Rich Presence", value: string("richPresence", fallback: settings.localized("Active")), localizeValue: false)
                        }
                    } else {
                        Text(settings.localized("Boot a game while RetroAchievements is enabled to see game progress here."))
                            .foregroundStyle(.secondary)
                    }
                }
            } else {
                Section(settings.localized("Status")) {
                    statusRow("Status", value: "Temporarily Unavailable")
                    Text(settings.localized(unavailableMessage))
                        .foregroundStyle(.secondary)
                }
            }

            Section {
                Text(settings.localized("Per-game RetroAchievements overrides can be set from the per-game settings panel."))
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
        .navigationTitle(settings.localized("RetroAchievements"))
        .navigationBarTitleDisplayMode(.inline)
        .onAppear(perform: refresh)
        .onReceive(NotificationCenter.default.publisher(for: retroAchievementsNotification)) { _ in
            refresh()
        }
        .sheet(isPresented: $showingLogin) {
            RetroAchievementsLoginSheet(
                username: $username,
                password: $password,
                loggingIn: loggingIn,
                onLogin: beginLogin,
                onCancel: {
                    guard !loggingIn else { return }
                    password = ""
                    showingLogin = false
                }
            )
        }
        .alert(settings.localized(messageTitle), isPresented: $showingMessage) {
            Button(settings.localized("OK"), role: .cancel) {}
        } message: {
            Text(messageBody)
        }
    }

    private var accountSummary: String {
        if bool("loggedIn") {
            return displayName
        }
        if bool("savedLogin") {
            return displayName
        }
        return achievementsEnabled ? "Not logged in" : "Disabled"
    }

    private var displayName: String {
        string("displayName", fallback: string("username", fallback: "Logged in"))
    }

    private var achievementsSupported: Bool {
        bool("supported", fallback: true)
    }

    private var hardcoreSupported: Bool {
        bool("hardcoreSupported", fallback: true)
    }

    private var hasStoredAccount: Bool {
        bool("loggedIn") || bool("savedLogin")
    }

    private var unavailableMessage: String {
        string("unavailableMessage", fallback: "RetroAchievements is temporarily unavailable in this build.")
    }

    private var hardcoreStatus: String {
        guard achievementsEnabled else { return "Disabled" }
        if hardcoreEnabled && !bool("hasActiveGame") { return "Ready" }
        if bool("hardcoreActive") { return "Active" }
        if hardcoreEnabled { return "Pending Reset" }
        return "Off"
    }

    private func statusRow(_ title: String, value: String, localizeValue: Bool = true) -> some View {
        HStack(alignment: .firstTextBaseline) {
            Text(settings.localized(title))
            Spacer(minLength: 16)
            Text(localizeValue ? settings.localized(value) : value)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.trailing)
        }
    }

    private func refresh() {
        state = ARMSX2Bridge.retroAchievementsState()
        achievementsEnabled = bool("enabled")
        // The in-memory EmuConfig.Achievements.HardcoreMode (exposed via
        // "hardcorePreference") can lag the persisted INI when no game has
        // booted yet, so the home-screen toggle showed stale state. Fall back
        // to the persisted Achievements/ChallengeMode INI value in that case so
        // the toggle reflects what was last saved.
        if bool("hardcoreActive") {
            hardcoreEnabled = bool("hardcorePreference")
        } else {
            hardcoreEnabled = ARMSX2Bridge.getINIBool("Achievements", key: "ChallengeMode", defaultValue: false)
        }
        notificationsEnabled = bool("notifications", fallback: true)
        leaderboardNotificationsEnabled = bool("leaderboardNotifications", fallback: true)
        overlaysEnabled = bool("overlays", fallback: true)
    }

    private func refreshSoon() {
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.25) {
            refresh()
        }
    }

    private func beginLogin() {
        let trimmedUsername = username.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmedUsername.isEmpty, !password.isEmpty else {
            showMessage(title: "Missing Login", body: settings.localized("Enter both your RetroAchievements username and password."))
            return
        }

        loggingIn = true
        NSLog("@@RA_LOGIN_UI@@ begin username_present=1")
        ARMSX2Bridge.loginRetroAchievements(username: trimmedUsername, password: password) { success, message in
            Task { @MainActor in
                loggingIn = false
                password = ""
                showingLogin = false
                refresh()
                NSLog("@@RA_LOGIN_UI@@ complete success=\(success ? 1 : 0)")
                showMessage(title: success ? "Logged In" : "Login Failed", body: settings.localized(message))
            }
        }
    }

    private func showMessage(title: String, body: String) {
        messageTitle = title
        messageBody = body
        showingMessage = true
    }

    private func bool(_ key: String, fallback: Bool = false) -> Bool {
        state[key] as? Bool ?? fallback
    }

    private func int(_ key: String, fallback: Int = 0) -> Int {
        if let value = state[key] as? Int {
            return value
        }
        if let value = state[key] as? NSNumber {
            return value.intValue
        }
        return fallback
    }

    private func string(_ key: String, fallback: String = "") -> String {
        guard let value = state[key] as? String, !value.isEmpty else {
            return fallback
        }
        return value
    }
}

private struct RetroAchievementsLoginSheet: View {
    @State private var settings = SettingsStore.shared
    @Binding var username: String
    @Binding var password: String
    let loggingIn: Bool
    let onLogin: () -> Void
    let onCancel: () -> Void

    private var canSubmit: Bool {
        !loggingIn &&
        !username.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty &&
        !password.isEmpty
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField(settings.localized("Username"), text: $username)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                        .textContentType(.username)
                        .submitLabel(.next)

                    SecureField(settings.localized("Password"), text: $password)
                        .textContentType(.password)
                        .submitLabel(.go)
                        .onSubmit {
                            if canSubmit {
                                onLogin()
                            }
                        }
                } footer: {
                    Text(settings.localized("Use your RetroAchievements account credentials."))
                }

                if loggingIn {
                    Section {
                        HStack {
                            ProgressView()
                            Text(settings.localized("Logging In..."))
                        }
                    }
                }
            }
            .navigationTitle(settings.localized("RetroAchievements Login"))
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button(settings.localized("Cancel"), action: onCancel)
                        .disabled(loggingIn)
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button(settings.localized("Log In"), action: onLogin)
                        .disabled(!canSubmit)
                }
            }
        }
        .interactiveDismissDisabled(loggingIn)
        .presentationDetents([.medium, .large])
    }
}
