// AppLanguage.swift - app language model and localization data for SwiftUI
// SPDX-License-Identifier: GPL-3.0+

import Foundation
import SwiftUI

enum AppLanguage: String, CaseIterable, Identifiable {
    case system
    case english
    case simplifiedChinese
    case arabic
    case spanish
    case french
    case german
    case italian
    case portuguese
    case japanese
    case korean

    var id: String { rawValue }

    var label: String {
        switch self {
        case .system: return "System Default"
        case .english: return "English"
        case .simplifiedChinese: return "简体中文"
        case .arabic: return "العربية"
        case .spanish: return "Español"
        case .french: return "Français"
        case .german: return "Deutsch"
        case .italian: return "Italiano"
        case .portuguese: return "Português"
        case .japanese: return "日本語"
        case .korean: return "한국어"
        }
    }

    static func resolvedSystemLanguage() -> AppLanguage {
        let code = Locale.current.language.languageCode?.identifier.lowercased() ?? "en"
        switch code {
        case "zh":
            return .simplifiedChinese
        case "ar":
            return .arabic
        case "es":
            return .spanish
        case "fr":
            return .french
        case "de":
            return .german
        case "it":
            return .italian
        case "pt":
            return .portuguese
        case "ja":
            return .japanese
        case "ko":
            return .korean
        default:
            return .english
        }
    }

    var resolved: AppLanguage {
        self == .system ? Self.resolvedSystemLanguage() : self
    }

    var layoutDirection: LayoutDirection {
        resolved == .arabic ? .rightToLeft : .leftToRight
    }

    func localized(_ key: String) -> String {
        guard let translated = Self.translations[resolved]?[key] else {
            return Self.commonTranslations[resolved]?[key] ?? Self.uiSupplementTranslations[resolved]?[key] ?? key
        }
        return translated
    }

}
