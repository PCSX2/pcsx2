// SPDX-License-Identifier: GPL-3.0+

import Foundation
import UIKit

@MainActor
enum ARMSX2DeepLinkHandler {
    private static let supportedSchemes: Set<String> = ["armsx2", "armsx2-ios", "armsx2ios"]

    @discardableResult
    static func handle(_ url: URL) -> Bool {
        guard let scheme = url.scheme?.lowercased(), supportedSchemes.contains(scheme) else {
            return false
        }

        let route = routeComponents(for: url)
        if route.contains("library") || route.contains("export-library") || route.contains("games") {
            exportLibrary(from: url)
            return true
        }

        if route.contains("launch") || route.contains("boot") || route.contains("play") {
            launchGame(from: url)
            return true
        }

        showMessage("Unsupported ARMSX2 link: \(url.absoluteString)")
        NSLog("[ARMSX2 iOS DeepLink] unsupported url=%@", url.absoluteString)
        return true
    }

    private static func routeComponents(for url: URL) -> Set<String> {
        var components: [String] = []
        if let host = url.host, !host.isEmpty {
            components.append(host)
        }
        components.append(contentsOf: url.pathComponents.filter { $0 != "/" })
        return Set(components.map { $0.lowercased() })
    }

    private static func queryValue(_ names: [String], in url: URL) -> String? {
        if let components = URLComponents(url: url, resolvingAgainstBaseURL: false) {
            for name in names {
                if let value = components.queryItems?.first(where: { $0.name.caseInsensitiveCompare(name) == .orderedSame })?.value,
                   !value.isEmpty {
                    return value
                }
            }
        }

        // Some frontends send callback URLs without percent-encoding (for example,
        // callback=ludihub://armsx2-callback). Parse the raw query as a fallback.
        let wantedNames = Set(names.map { $0.lowercased() })
        guard let query = url.query else {
            return nil
        }

        for item in query.split(separator: "&", omittingEmptySubsequences: false) {
            let parts = item.split(separator: "=", maxSplits: 1, omittingEmptySubsequences: false)
            guard parts.count == 2 else {
                continue
            }

            let key = String(parts[0]).removingPercentEncoding ?? String(parts[0])
            guard wantedNames.contains(key.lowercased()) else {
                continue
            }

            let rawValue = String(parts[1])
            let value = rawValue.removingPercentEncoding ?? rawValue
            if !value.isEmpty {
                return value
            }
        }

        return nil
    }

    private static func exportLibrary(from url: URL) {
        guard let callback = queryValue(["callback", "callback_url", "return", "return_url", "x-success"], in: url) else {
            showMessage("LudiHub library export requested, but no callback URL was provided.")
            NSLog("[ARMSX2 iOS DeepLink] library export missing callback url=%@", url.absoluteString)
            return
        }

        let payload = libraryPayload()
        do {
            let data = try JSONSerialization.data(withJSONObject: payload, options: [.sortedKeys])
            let encoded = base64URLEncoded(data)
            guard let callbackURL = callbackURL(base: callback, payload: encoded) else {
                showMessage("LudiHub callback URL was invalid.")
                NSLog("[ARMSX2 iOS DeepLink] invalid callback=%@", callback)
                return
            }

            NSLog("[ARMSX2 iOS DeepLink] exporting library count=%d callback=%@", payload.gameCount, callbackURL.absoluteString)
            UIApplication.shared.open(callbackURL, options: [:]) { didOpen in
                NSLog("[ARMSX2 iOS DeepLink] callback open result=%d callback=%@", didOpen ? 1 : 0, callbackURL.absoluteString)
                if !didOpen {
                    Task { @MainActor in
                        showMessage("LudiHub callback could not be opened.")
                    }
                }
            }
        } catch {
            showMessage("LudiHub library export failed: \(error.localizedDescription)")
            NSLog("[ARMSX2 iOS DeepLink] library export failed: %@", error.localizedDescription)
        }
    }

    private static func libraryPayload() -> [String: Any] {
        let isoDir = URL(fileURLWithPath: ARMSX2Bridge.isoDirectory())
        let docsDir = URL(fileURLWithPath: ARMSX2Bridge.documentsDirectory())
        let gameNames = ARMSX2Bridge.availableISOs()
            .filter { !$0.lowercased().hasSuffix(".elf") }
            .sorted { $0.localizedCaseInsensitiveCompare($1) == .orderedAscending }

        let games: [[String: Any]] = gameNames.map { isoName in
            let metadata = ARMSX2Bridge.gameMetadata(forISO: isoName)
            let title = metadata["title"] ?? metadata["fileTitle"] ?? URL(fileURLWithPath: isoName).deletingPathExtension().lastPathComponent
            let isoURL = resolvedISOURL(isoName: isoName, isoDir: isoDir, docsDir: docsDir)
            let fileSize = fileSize(at: isoURL)
            let launchURL = "armsx2://launch?game=\(percentEncoded(isoName))"

            return [
                "title": title,
                "fileName": isoName,
                "serial": metadata["serial"] ?? "",
                "region": metadata["region"] ?? "",
                "crc": metadata["crc"] ?? "",
                "fileSize": fileSize,
                "fileType": isoURL.pathExtension.uppercased(),
                "launchURL": launchURL
            ]
        }

        return [
            "schema": "com.armsx2.library.v1",
            "app": "ARMSX2 iOS",
            "version": ARMSX2Bridge.buildVersion(),
            "generatedAt": ISO8601DateFormatter().string(from: Date()),
            "gameCount": games.count,
            "games": games
        ]
    }

    private static func resolvedISOURL(isoName: String, isoDir: URL, docsDir: URL) -> URL {
        let isoURL = isoDir.appendingPathComponent(isoName)
        if FileManager.default.fileExists(atPath: isoURL.path) {
            return isoURL
        }
        return docsDir.appendingPathComponent(isoName)
    }

    private static func fileSize(at url: URL) -> Int64 {
        guard let value = try? FileManager.default.attributesOfItem(atPath: url.path)[.size] as? NSNumber else {
            return 0
        }
        return value.int64Value
    }

    private static func launchGame(from url: URL) {
        guard let game = queryValue(["game", "iso", "file", "name"], in: url)?.removingPercentEncoding else {
            showMessage("ARMSX2 launch link is missing a game filename.")
            NSLog("[ARMSX2 iOS DeepLink] launch missing game url=%@", url.absoluteString)
            return
        }

        let available = Set(ARMSX2Bridge.availableISOs())
        guard available.contains(game) else {
            showMessage("ARMSX2 could not find \(game).")
            NSLog("[ARMSX2 iOS DeepLink] launch missing local game=%@", game)
            return
        }

        NSLog("[ARMSX2 iOS DeepLink] launching game=%@", game)
        AppState.shared.bootGame(isoName: game)
    }

    private static func callbackURL(base: String, payload: String) -> URL? {
        let decodedBase = base.removingPercentEncoding ?? base
        let candidates = decodedBase == base ? [base] : [base, decodedBase]

        for candidate in candidates {
            guard var components = URLComponents(string: candidate) else {
                continue
            }

            var items = components.queryItems ?? []
            items.append(URLQueryItem(name: "source", value: "armsx2-ios"))
            items.append(URLQueryItem(name: "payload", value: payload))
            components.queryItems = items
            if let url = components.url {
                return url
            }
        }

        return nil
    }

    private static func base64URLEncoded(_ data: Data) -> String {
        data.base64EncodedString()
            .replacingOccurrences(of: "+", with: "-")
            .replacingOccurrences(of: "/", with: "_")
            .replacingOccurrences(of: "=", with: "")
    }

    private static func percentEncoded(_ value: String) -> String {
        value.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed) ?? value
    }

    private static func showMessage(_ message: String) {
        FileImportHandler.shared.presentImportResult(message)
    }
}

private extension Dictionary where Key == String, Value == Any {
    var gameCount: Int {
        self["gameCount"] as? Int ?? 0
    }
}

@objc class DeepLinkBridge: NSObject {
    @objc @discardableResult
    static func handle(_ url: URL) -> Bool {
        Task { @MainActor in
            _ = ARMSX2DeepLinkHandler.handle(url)
        }
        return true
    }
}
