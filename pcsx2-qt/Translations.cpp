// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "MainWindow.h"
#include "QtHost.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SmallString.h"
#include "common/StringUtil.h"

#include "pcsx2/ImGui/FullscreenUI.h"
#include "pcsx2/ImGui/ImGuiManager.h"
#include "pcsx2/MTGS.h"

#include "fmt/format.h"
#include "imgui.h"

#include <QtCore/QCollator>
#include <QtCore/QFile>
#include <QtCore/QTranslator>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QMessageBox>

#include <optional>
#include <vector>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <KnownFolders.h>
#include <ShlObj.h>
#endif

#if 0
// Qt internal strings we'd like to have translated
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Services")
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Hide %1")
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Hide Others")
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Show All")
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Preferences...")
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Quit %1")
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "About %1")

// Strings that will be parsed out by our build system and sent to fun places
QT_TRANSLATE_NOOP("PermissionsDialogMicrophone", "PCSX2 uses your microphone to emulate a USB microphone plugged into the virtual PS2.")
QT_TRANSLATE_NOOP("PermissionsDialogCamera", "PCSX2 uses your camera to emulate an EyeToy camera plugged into the virtual PS2.")
#endif

namespace QtHost
{
	struct FontInfo
	{
		const char* language;
		const char* imgui_font_name;
	};

	static void UpdateGlyphRangesAndClearCache(QWidget* dialog_parent, const std::string_view language);
	static bool DownloadMissingFont(QWidget* dialog_parent, const char* font_name, const std::string& path);
	static const FontInfo* GetFontInfo(const std::string_view language);

	static constexpr const char* DEFAULT_IMGUI_FONT_NAME = "Roboto-Regular.ttf";

	static QLocale s_current_locale;
	static QCollator s_current_collator;

	static std::vector<QTranslator*> s_translators;
} // namespace QtHost

static QString getSystemLanguage()
{
	std::vector<std::pair<QString, QString>> available = QtHost::GetAvailableLanguageList();
	QString locale = QLocale::system().name();
	locale.replace('_', '-');
	// Can we find an exact match?
	for (const std::pair<QString, QString>& entry : available)
	{
		if (entry.second == locale)
			return locale;
	}
	// How about a partial match?
	QStringView lang = QStringView(locale);
	lang = lang.left(lang.indexOf('-'));
	for (const std::pair<QString, QString>& entry : available)
	{
		QStringView avail = QStringView(entry.second);
		avail = avail.left(avail.indexOf('-'));
		if (avail == lang)
		{
			Console.Warning("Couldn't find translation for system language %s, using %s instead",
				locale.toStdString().c_str(), entry.second.toStdString().c_str());
			return entry.second;
		}
	}
	// No matches :(
	Console.Warning("Couldn't find translation for system language %s, using en instead", locale.toStdString().c_str());
	return QStringLiteral("en-US");
}

void QtHost::InstallTranslator(QWidget* dialog_parent)
{
	for (QTranslator* translator : s_translators)
	{
		qApp->removeTranslator(translator);
		translator->deleteLater();
	}
	s_translators.clear();

	QString language =
		QString::fromStdString(Host::GetBaseStringSettingValue("UI", "Language", GetDefaultLanguage()));
	if (language == QStringLiteral("system"))
		language = getSystemLanguage();

	QString qlanguage = language;
	qlanguage.replace('-', '_');
	s_current_locale = QLocale(qlanguage);
	s_current_collator = QCollator(s_current_locale);

	// Install the base qt translation first.
#if defined(__APPLE__)
	const QString base_dir = QStringLiteral("%1/../Resources/translations").arg(qApp->applicationDirPath());
#elif defined(PCSX2_APP_DATADIR)
	const QString base_dir = QStringLiteral("%1/%2/translations").arg(qApp->applicationDirPath()).arg(PCSX2_APP_DATADIR);
#else
	const QString base_dir = QStringLiteral("%1/translations").arg(qApp->applicationDirPath());
#endif

	// Qt base uses underscores instead of hyphens.
	const QString qt_language = QString(language).replace(QChar('-'), QChar('_'));
	QString base_path = QStringLiteral("%1/qt_%2.qm").arg(base_dir).arg(qt_language);
	bool has_base_ts = QFile::exists(base_path);
	if (!has_base_ts)
	{
		// Try without the country suffix.
		const qsizetype index = language.lastIndexOf('-');
		if (index > 0)
		{
			base_path = QStringLiteral("%1/qt_%2.qm").arg(base_dir).arg(language.left(index));
			has_base_ts = QFile::exists(base_path);
		}
	}

	if (has_base_ts)
	{
		QTranslator* base_translator = new QTranslator(qApp);
		if (!base_translator->load(base_path))
		{
			QMessageBox::warning(nullptr, QStringLiteral("Translation Error"),
				QStringLiteral("Failed to find load base translation file for '%1':\n%2").arg(language).arg(base_path));
			delete base_translator;
		}
		else
		{
			s_translators.push_back(base_translator);
			qApp->installTranslator(base_translator);
		}
	}

	const QString path = QStringLiteral("%1/pcsx2-qt_%3.qm").arg(base_dir).arg(language);
	QTranslator* translator = nullptr;
	if (QFile::exists(path))
	{
		translator = new QTranslator(qApp);
		if (translator->load(path))
		{
			Console.WriteLn(
				Color_StrongYellow, "Loaded translation file for language %s", language.toUtf8().constData());
		}
		else
		{
			QMessageBox::warning(nullptr, QStringLiteral("Translation Error"),
				QStringLiteral("Failed to load translation file for language '%1':\n%2").arg(language).arg(path));
			delete translator;
			translator = nullptr;
		}
	}
	else
	{
#ifdef PCSX2_DEVBUILD
		// For now, until we're sure this works on all platforms, we won't block users from starting if they're missing.
		QMessageBox::warning(nullptr, QStringLiteral("Translation Error"),
			QStringLiteral("Failed to find translation file for language '%1':\n%2").arg(language).arg(path));
#endif
	}

	if (translator)
	{
		qApp->installTranslator(translator);
		s_translators.push_back(translator);
	}

	UpdateGlyphRangesAndClearCache(dialog_parent, language.toStdString());

	if (FullscreenUI::IsInitialized())
	{
		MTGS::RunOnGSThread([]() mutable {
			FullscreenUI::LocaleChanged();
		});
	}
}

const char* QtHost::GetDefaultLanguage()
{
	return "system";
}

s32 Host::Internal::GetTranslatedStringImpl(
	const std::string_view context, const std::string_view msg, char* tbuf, size_t tbuf_space)
{
	// This is really awful. Thankfully we're caching the results...
	const std::string temp_context(context);
	const std::string temp_msg(msg);
	const QString translated_msg = qApp->translate(temp_context.c_str(), temp_msg.c_str());
	const QByteArray translated_utf8 = translated_msg.toUtf8();
	const size_t translated_size = translated_utf8.size();
	if (translated_size > tbuf_space)
		return -1;
	else if (translated_size > 0)
		std::memcpy(tbuf, translated_utf8.constData(), translated_size);

	return static_cast<s32>(translated_size);
}

std::string Host::TranslatePluralToString(const char* context, const char* msg, const char* disambiguation, int count)
{
	return qApp->translate(context, msg, disambiguation, count).toStdString();
}

bool Host::LocaleCircleConfirm()
{
	QLocale& loc = QtHost::s_current_locale;
	return (loc.language() == QLocale::Japanese) || (loc.language() == QLocale::Chinese) || (loc.language() == QLocale::Korean);
}

std::vector<std::pair<QString, QString>> QtHost::GetAvailableLanguageList()
{
	return {
		{QCoreApplication::translate("InterfaceSettingsWidget", "System Language [Default]"), QStringLiteral("system")},
		{QStringLiteral("Afrikaans (af-ZA)"), QStringLiteral("af-ZA")},
		{QStringLiteral("عربي (ar-SA)"), QStringLiteral("ar-SA")},
		{QStringLiteral("Català (ca-ES)"), QStringLiteral("ca-ES")},
		{QStringLiteral("Čeština (cs-CZ)"), QStringLiteral("cs-CZ")},
		{QStringLiteral("Dansk (da-DK)"), QStringLiteral("da-DK")},
		{QStringLiteral("Deutsch (de-DE)"), QStringLiteral("de-DE")},
		{QStringLiteral("Ελληνικά (el-GR)"), QStringLiteral("el-GR")},
		{QStringLiteral("English (en)"), QStringLiteral("en-US")},
		{QStringLiteral("Español (Hispanoamérica) (es-419)"), QStringLiteral("es-419")},
		{QStringLiteral("Español (España) (es-ES)"), QStringLiteral("es-ES")},
		{QStringLiteral("فارسی (fa-IR)"), QStringLiteral("fa-IR")},
		{QStringLiteral("Suomi (fi-FI)"), QStringLiteral("fi-FI")},
		{QStringLiteral("Français (fr-FR)"), QStringLiteral("fr-FR")},
		{QStringLiteral("עִבְרִית (he-IL)"), QStringLiteral("he-IL")},
		{QStringLiteral("मानक हिन्दी (hi-IN)"), QStringLiteral("hi-IN")},
		{QStringLiteral("Magyar (hu-HU)"), QStringLiteral("hu-HU")},
		{QStringLiteral("hrvatski (hr-HR)"), QStringLiteral("hr-HR")},
		{QStringLiteral("Bahasa Indonesia (id-ID)"), QStringLiteral("id-ID")},
		{QStringLiteral("Italiano (it-IT)"), QStringLiteral("it-IT")},
		{QStringLiteral("日本語 (ja-JP)"), QStringLiteral("ja-JP")},
		{QStringLiteral("한국어 (ko-KR)"), QStringLiteral("ko-KR")},
		{QStringLiteral("Latvija (lv-LV)"), QStringLiteral("lv-LV")},
		{QStringLiteral("Lietuvių (lt-LT)"), QStringLiteral("lt-LT")},
		{QStringLiteral("Nederlands (nl-NL)"), QStringLiteral("nl-NL")},
		{QStringLiteral("Norsk (no-NO)"), QStringLiteral("no-NO")},
		{QStringLiteral("Polski (pl-PL)"), QStringLiteral("pl-PL")},
		{QStringLiteral("Português (Brasil) (pt-BR)"), QStringLiteral("pt-BR")},
		{QStringLiteral("Português (Portugal) (pt-PT)"), QStringLiteral("pt-PT")},
		{QStringLiteral("Limba română (ro-RO)"), QStringLiteral("ro-RO")},
		{QStringLiteral("Русский (ru-RU)"), QStringLiteral("ru-RU")},
		{QStringLiteral("Српски језик (sr-SP)"), QStringLiteral("sr-SP")},
		{QStringLiteral("Svenska (sv-SE)"), QStringLiteral("sv-SE")},
		{QStringLiteral("Türkçe (tr-TR)"), QStringLiteral("tr-TR")},
		{QStringLiteral("Українська мова (uk-UA)"), QStringLiteral("uk-UA")},
		{QStringLiteral("Tiếng Việt (vi-VN)"), QStringLiteral("vi-VN")},
		{QStringLiteral("简体中文 (zh-CN)"), QStringLiteral("zh-CN")},
		{QStringLiteral("繁體中文 (zh-TW)"), QStringLiteral("zh-TW")},
	};
}

void QtHost::UpdateGlyphRangesAndClearCache(QWidget* dialog_parent, const std::string_view language)
{
	const FontInfo* fi = GetFontInfo(language);

	const char* imgui_font_name = nullptr;

	if (fi)
		imgui_font_name = fi->imgui_font_name;

	// Check for the presence of font files.
	std::string font_path;
	if (imgui_font_name)
	{
		// Non-standard fonts always go to the user resources directory, since they're downloaded on demand.
		font_path = Path::Combine(EmuFolders::UserResources,
			SmallString::from_format("fonts" FS_OSPATH_SEPARATOR_STR "{}", imgui_font_name));
		if (!DownloadMissingFont(dialog_parent, imgui_font_name, font_path))
			font_path.clear();
	}
	if (font_path.empty())
	{
		// Use the default font.
		font_path = EmuFolders::GetOverridableResourcePath(SmallString::from_format(
			"fonts" FS_OSPATH_SEPARATOR_STR "{}", DEFAULT_IMGUI_FONT_NAME));
	}

	// Called on UI thread, so we need to do this on the CPU/GS thread if it's active.
	if (g_emu_thread)
	{
		Host::RunOnCPUThread([font_path = std::move(font_path)]() mutable {
			if (MTGS::IsOpen())
			{
				MTGS::RunOnGSThread([font_path = std::move(font_path)]() mutable {
					ImGuiManager::SetFontPath(std::move(font_path));
				});
			}
			else
			{
				ImGuiManager::SetFontPath(std::move(font_path));
			}

			Host::ClearTranslationCache();
		});
	}
	else
	{
		// Startup, safe to set directly.
		ImGuiManager::SetFontPath(std::move(font_path));
		Host::ClearTranslationCache();
	}
}

bool QtHost::DownloadMissingFont(QWidget* dialog_parent, const char* font_name, const std::string& path)
{
	if (FileSystem::FileExists(path.c_str()))
		return true;

	{
		QMessageBox msgbox(dialog_parent);
		msgbox.setWindowTitle(qApp->translate("MainWindow", "Missing Font File"));
		msgbox.setWindowIcon(QtHost::GetAppIcon());
		msgbox.setWindowModality(Qt::WindowModal);
		msgbox.setIcon(QMessageBox::Critical);
		msgbox.setTextFormat(Qt::RichText);
		msgbox.setText(qApp->translate("MainWindow",
			"The font file '%1' is required for the On-Screen Display and Big Picture Mode to show messages in your language.<br><br>"
			"Do you want to download this file now? These files are usually less than 10 megabytes in size.<br><br>"
			"<strong>If you do not download this file, on-screen messages will not be readable.</strong>")
			.arg(QLatin1StringView(font_name)));
		msgbox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		if (msgbox.exec() != QMessageBox::Yes)
			return false;
	}

	const QString progress_title = qApp->translate("MainWindow", "Downloading Files");
	std::string url = QtHost::GetRuntimeDownloadedResourceURL(font_name);
	return QtHost::DownloadFile(dialog_parent, progress_title, std::move(url), path);
}

static constexpr const QtHost::FontInfo s_font_info[] = {
	{"ar-SA", "NotoSansArabic-Regular.ttf"},
	{"fa-IR", "NotoSansArabic-Regular.ttf"},
	{"hi-IN", "NotoSansDevanagari-Regular.ttf"},
	{"he-IL", "NotoSansHebrew-Regular.ttf"},
	{"ja-JP", "NotoSansJP-Regular.ttf"},
	{"ko-KR", "NotoSansKR-Regular.ttf"},
	{"zh-CN", "NotoSansSC-Regular.ttf"},
	{"zh-TW", "NotoSansTC-Regular.ttf"},
};

const QtHost::FontInfo* QtHost::GetFontInfo(const std::string_view language)
{
	for (const FontInfo& it : s_font_info)
	{
		if (language == it.language)
			return &it;
	}

	return nullptr;
}

int QtHost::LocaleSensitiveCompare(QStringView lhs, QStringView rhs)
{
	return s_current_collator.compare(lhs, rhs);
}
