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

#if defined(_WIN32)
#include "common/RedtapeWindows.h"
#include <KnownFolders.h>
#include <ShlObj.h>
#elif defined(__APPLE__)
#include <CoreText/CoreText.h>
#else
#include <fontconfig/fontconfig.h>
#define USE_FONTCONFIG
#endif

#ifdef USE_FONTCONFIG
typedef FcConfig* FontSearchContext;
static void FontSearchContextDestroy(FcConfig* config) { if (config) FcConfigDestroy(config); }
#else
typedef void* FontSearchContext;
static void FontSearchContextDestroy(void*) {}
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

enum class FontScript : u8;

namespace QtHost
{
	static void UpdateGlyphRangesAndClearCache(QWidget* dialog_parent, const std::string_view language);
	static bool DownloadMissingFont(QWidget* dialog_parent, const char* font_name, const std::string& path);
	static FontScript GetFontScript(const std::string_view language);

	static QLocale s_current_locale;
	static QCollator s_current_collator;
	static std::mutex s_collator_mtx;

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
	{
		std::lock_guard<std::mutex> guard(s_collator_mtx);
		s_current_collator = QCollator(s_current_locale);
	}

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

enum class FontScript : u8
{
	Arabic,
	ChineseSimplified,
	ChineseTraditional,
	Devanagari,
	Emoji,
	Hebrew,
	Japanese,
	Korean,
	Latin,
};
static constexpr size_t NumFontScripts = static_cast<size_t>(FontScript::Latin) + 1;

struct FontLoadInfo
{
	const char* file_name = nullptr;
	const char* face_name = nullptr;
};

namespace FontNames
{
	static constexpr FontLoadInfo Arabic[] = {
		{"NotoSansArabic-Regular.ttf"},
		{"Segoeui.ttf"},
		{nullptr, "Geeza Pro"},
		{nullptr, "Noto Sans Arabic"},
	};
	static constexpr FontLoadInfo ChineseSimplified[] = {
		{"NotoSansSC-Regular.ttf"},
		{"Msyh.ttc"},
		// {nullptr, "PingFang SC"}, // Freetype fails to load PingFang ttc
		{nullptr, "Heiti SC"},
		{nullptr, "Noto Sans CJK SC"},
	};
	static constexpr FontLoadInfo ChineseTraditional[] = {
		{"NotoSansTC-Regular.ttf"},
		{"Msjh.ttc"},
		// {nullptr, "PingFang TC"}, // Freetype fails to load PingFang ttc
		{nullptr, "Heiti TC"},
		{nullptr, "Noto Sans CJK TC"},
	};
	static constexpr FontLoadInfo Devanagari[] = {
		{"NotoSansDevanagari-Regular.ttf"},
		{"Nirmala.ttf"},
		{nullptr, "Kohinoor Devanagari"},
		{nullptr, "Noto Sans Devanagari"},
	};
	static constexpr FontLoadInfo Emoji[] = {
		{"NotoColorEmoji-Regular.ttf"},
		{"Seguiemj.ttf"},
		// {nullptr, "Apple Color Emoji"}, // Freetype can't properly render Apple Color Emoji.
		{nullptr, "Noto Color Emoji"},
		{nullptr, "Noto Emoji"},
	};
	static constexpr FontLoadInfo Hebrew[] = {
		{"NotoSansHebrew-Regular.ttf"},
		{"Segoeui.ttf"},
		{nullptr, "Arial Hebrew"},
		{nullptr, "Noto Sans Hebrew"},
	};
	static constexpr FontLoadInfo Japanese[] = {
		{"NotoSansJP-Regular.ttf"},
		{"YuGothR.ttc"},
		{"Meiryo.ttc"},
		{nullptr, "Hiragino Sans"},
		{nullptr, "Noto Sans CJK JP"},
	};
	static constexpr FontLoadInfo Korean[] = {
		{"NotoSansKR-Regular.ttf"},
		{"Malgun.ttf"},
		{nullptr, "Apple SD Gothic Neo"},
		{nullptr, "Noto Sans CJK KR"},
	};
	static constexpr FontLoadInfo Latin[] = {
		// We ship this with PCSX2 so no fallbacks are needed
		{"Roboto-Regular.ttf"},
	};
} // namespace FontNames

static constexpr std::span<const FontLoadInfo> GetFontNames(FontScript script)
{
	switch (script)
	{
#define CASE(x) case FontScript::x: return FontNames::x
		CASE(Arabic);
		CASE(ChineseSimplified);
		CASE(ChineseTraditional);
		CASE(Devanagari);
		CASE(Emoji);
		CASE(Hebrew);
		CASE(Japanese);
		CASE(Korean);
		CASE(Latin);
#undef CASE
	}
}

static constexpr std::array<std::span<const FontLoadInfo>, NumFontScripts> g_font_load_info = [] {
	std::array<std::span<const FontLoadInfo>, NumFontScripts> res = {};
	for (size_t i = 0; i < NumFontScripts; i++)
		res[i] = GetFontNames(static_cast<FontScript>(i));
	return res;
}();

struct FontData
{
	std::span<const u8> data;
	const char* face_name;
	operator bool() const { return !data.empty(); }
};

static FontData s_font_data[NumFontScripts];

static SmallString GetFontPath(const char* name)
{
	SmallString path = "fonts" FS_OSPATH_SEPARATOR_STR;
	path.append(name);
	return path;
}

static std::span<const u8> TryLoadFont(FontSearchContext* ctx, const FontLoadInfo& info)
{
	if (info.file_name)
	{
		SmallString path = GetFontPath(info.file_name);
		std::string downloaded = Path::Combine(EmuFolders::UserResources, path);
		if (std::span<const u8> data = FileSystem::MapBinaryFileForRead(downloaded.c_str()); !data.empty())
			return data;
		std::string shipped = EmuFolders::GetOverridableResourcePath(path);
		if (std::span<const u8> data = FileSystem::MapBinaryFileForRead(shipped.c_str()); !data.empty())
			return data;
	}
#if defined(_WIN32)
	if (!info.file_name)
		return {};
	SmallString path = "C:\\Windows\\Fonts\\";
	path.append(info.file_name);
	return FileSystem::MapBinaryFileForRead(path.c_str());
#elif defined(__APPLE__)
	const char* name = info.face_name;
	if (!name)
		return {}; // Don't bother looking up file names
	std::span<const u8> res = {};
	CFStringRef cfname = CFStringCreateWithBytesNoCopy(nullptr, reinterpret_cast<const u8*>(name), strlen(name), kCFStringEncodingUTF8, false, kCFAllocatorNull);
	CTFontDescriptorRef desc = CTFontDescriptorCreateWithNameAndSize(cfname, 0);
	if (CFURLRef url = static_cast<CFURLRef>(CTFontDescriptorCopyAttribute(desc, kCTFontURLAttribute)))
	{
		u8 path[PATH_MAX];
		if (CFURLGetFileSystemRepresentation(url, true, path, sizeof(path)))
			res = FileSystem::MapBinaryFileForRead(reinterpret_cast<char*>(path));
		CFRelease(url);
	}
	CFRelease(desc);
	CFRelease(cfname);
	return res;
#else
	const char* name = info.face_name;
	if (!name)
		return {}; // Don't bother looking up file names
	if (!*ctx)
		*ctx = FcInitLoadConfigAndFonts();
	std::span<const u8> res = {};
	FcPattern* search = FcNameParse(reinterpret_cast<const FcChar8*>(name));
	FcResult fcres;
	FcPattern* font = FcFontMatch(*ctx, search, &fcres);
	if (font)
	{
		FcChar8* family;
		FcChar8* path;
		if (FcPatternGetString(font, FC_FILE, 0, &path) == FcResultMatch
		 && FcPatternGetString(font, FC_FAMILY, 0, &family) == FcResultMatch
		 && 0 == strcmp(name, reinterpret_cast<char*>(family)))
		{
			res = FileSystem::MapBinaryFileForRead(reinterpret_cast<char*>(path));
		}
		FcPatternDestroy(font);
	}
	FcPatternDestroy(search);
	return res;
#endif
}

static bool ValidateFont(const FontLoadInfo& info, std::span<const u8> data)
{
	if (info.face_name && 0 == strcmp(info.face_name, "Noto Color Emoji"))
	{
		// Noto Color Emoji comes in bitmap, SVG, and COLRv1 variants.
		// We can only render the SVG variant.
		if (data.size() < 8)
			return false;
		u32 magic_be;
		u16 num_tables_be;
		memcpy(&magic_be, &data[0], sizeof(u32));
		memcpy(&num_tables_be, &data[4], sizeof(u16));
		if (qFromBigEndian(magic_be) != 0x10000)
			return false;
		size_t num_tables = qFromBigEndian(num_tables_be);
		if (data.size() < num_tables * 16 + 12)
			return false;
		for (size_t i = 0; i < num_tables; i++)
		{
			// Check if there's an "SVG " table
			if (0 == memcmp(&data[i * 16 + 12], "SVG ", 4))
				return true;
		}
		return false;
	}
	return true;
}

static void TryLoadFonts(FontSearchContext* ctx)
{
	for (size_t i = 0; i < NumFontScripts; i++)
	{
		if (!s_font_data[i].data.empty())
			continue;

		for (const FontLoadInfo& info : g_font_load_info[i])
		{
			if (std::span<const u8> data = TryLoadFont(ctx, info); !data.empty())
			{
				if (!ValidateFont(info, data))
				{
					FileSystem::UnmapFile(data);
					continue;
				}
				s_font_data[i].data = data;
				s_font_data[i].face_name = info.face_name;
				break;
			}
		}
	}
}

static FontScript GetSecondaryScript(FontScript primary)
{
	switch (primary)
	{
		case FontScript::ChineseSimplified: return FontScript::ChineseTraditional;
		case FontScript::ChineseTraditional: return FontScript::ChineseSimplified;
		default: return primary;
	}
}

static constexpr FontScript g_fallback_list[] = {
	FontScript::Japanese,
	FontScript::Korean,
	FontScript::ChineseSimplified,
	FontScript::ChineseTraditional,
	FontScript::Devanagari,
	FontScript::Arabic,
	FontScript::Hebrew,
	FontScript::Emoji,
};

static bool HasCenteredElipsis(FontScript script)
{
	switch (script)
	{
		case FontScript::ChineseSimplified:
		case FontScript::ChineseTraditional:
		case FontScript::Japanese:
		case FontScript::Korean:
			return true;
		default:
			return false;
	}
}

static void DownloadFontIfMissing(FontSearchContext* ctx, QWidget* dialog_parent, FontScript script)
{
	if (s_font_data[static_cast<size_t>(script)])
		return;
	const char* name = g_font_load_info[static_cast<size_t>(script)][0].file_name; // Downloadable font is always first
	std::string path = Path::Combine(EmuFolders::UserResources, GetFontPath(name));
	if (QtHost::DownloadMissingFont(dialog_parent, name, path))
		TryLoadFonts(ctx);
}

void QtHost::UpdateGlyphRangesAndClearCache(QWidget* dialog_parent, const std::string_view language)
{
	FontScript scriptPrimary   = GetFontScript(language);
	FontScript scriptSecondary = GetSecondaryScript(scriptPrimary);
	FontSearchContext ctx = nullptr;

	TryLoadFonts(&ctx);

	DownloadFontIfMissing(&ctx, dialog_parent, scriptPrimary);
	DownloadFontIfMissing(&ctx, dialog_parent, FontScript::Emoji);

	FontSearchContextDestroy(ctx);

	std::vector<ImGuiManager::FontInfo> fonts;
	fonts.reserve(std::size(g_fallback_list) + 1);

	auto AddFont = [&fonts](FontScript script) -> ImGuiManager::FontInfo* {
		const FontData& font = s_font_data[static_cast<size_t>(script)];
		if (!font)
			return nullptr;
		ImGuiManager::FontInfo& res = fonts.emplace_back();
		res.data = font.data;
		res.face_name = font.face_name;
		res.is_emoji_font = script == FontScript::Emoji;
		return &res;
	};

	// Use latin script for its characters regardless of language
	ImGuiManager::FontInfo* latin = AddFont(FontScript::Latin);
	if (latin && scriptPrimary != FontScript::Latin)
	{
		// Ellipsis is vertically centered in e.g. Japanese fonts, use the main language's version instead of the latin one
		static constexpr uint32_t exclude_ellipsis[] = {0x2026, 0x2026};
		if (HasCenteredElipsis(scriptPrimary))
			latin->exclude_ranges = exclude_ellipsis;
	}

	if (scriptPrimary != FontScript::Latin)
		AddFont(scriptPrimary);
	if (scriptSecondary != scriptPrimary)
		AddFont(scriptSecondary);
	for (FontScript script : g_fallback_list)
	{
		if (script != scriptPrimary && script != scriptSecondary)
			AddFont(script);
	}

	// Called on UI thread, so we need to do this on the CPU/GS thread if it's active.
	if (g_emu_thread)
	{
		Host::RunOnCPUThread([fonts = std::move(fonts)]() mutable {
			if (MTGS::IsOpen())
			{
				MTGS::RunOnGSThread([fonts = std::move(fonts)]() mutable {
					ImGuiManager::SetFonts(std::move(fonts));
				});
			}
			else
			{
				ImGuiManager::SetFonts(std::move(fonts));
			}

			Host::ClearTranslationCache();
		});
	}
	else
	{
		// Startup, safe to set directly.
		ImGuiManager::SetFonts(std::move(fonts));
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

static constexpr const struct
{
	const char* name;
	FontScript script;
} s_language_scripts[] = {
	{"ar-SA", FontScript::Arabic},
	{"fa-IR", FontScript::Arabic},
	{"hi-IN", FontScript::Devanagari},
	{"he-IL", FontScript::Hebrew},
	{"ja-JP", FontScript::Japanese},
	{"ko-KR", FontScript::Korean},
	{"zh-CN", FontScript::ChineseSimplified},
	{"zh-TW", FontScript::ChineseTraditional},
};

FontScript QtHost::GetFontScript(const std::string_view language)
{
	for (const auto& it : s_language_scripts)
	{
		if (language == it.name)
			return it.script;
	}

	return FontScript::Latin;
}

int QtHost::LocaleSensitiveCompare(QStringView lhs, QStringView rhs)
{
	return s_current_collator.compare(lhs, rhs);
}

int Host::LocaleSensitiveCompare(std::string_view lhs, std::string_view rhs)
{
	QString qlhs = QString::fromUtf8(lhs.data(), lhs.size());
	QString qrhs = QString::fromUtf8(rhs.data(), rhs.size());
	std::lock_guard<std::mutex> guard(QtHost::s_collator_mtx);
	return QtHost::s_current_collator.compare(qlhs, qrhs);
}
