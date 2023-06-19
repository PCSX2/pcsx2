/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include "QtHost.h"

#include "common/Console.h"
#include "common/StringUtil.h"

#include "pcsx2/ImGui/ImGuiManager.h"

#include "fmt/format.h"
#include "imgui.h"

#include <QtCore/QFile>
#include <QtCore/QTranslator>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QMessageBox>

#include <vector>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <KnownFolders.h>
#include <ShlObj.h>
#endif

namespace QtHost
{
	static const ImWchar* GetGlyphRangesJapanese();
	static const ImWchar* GetGlyphRangesChinese();
	static std::string GetFontPath(const char* name);
} // namespace QtHost

static std::vector<QTranslator*> s_translators;

void QtHost::InstallTranslator()
{
	for (QTranslator* translator : s_translators)
	{
		qApp->removeTranslator(translator);
		translator->deleteLater();
	}
	s_translators.clear();

	const QString language =
		QString::fromStdString(Host::GetBaseStringSettingValue("UI", "Language", GetDefaultLanguage()));

	// Install the base qt translation first.
		const QString base_dir = QStringLiteral("%1/translations").arg(qApp->applicationDirPath());
	QString base_path = QStringLiteral("%1/qt_%2.qm").arg(base_dir).arg(language);
	bool has_base_ts = QFile::exists(base_path);
	if (!has_base_ts)
	{
		// Try without the country suffix.
		const int index = language.indexOf('-');
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
	if (!QFile::exists(path))
	{
#ifdef PCSX2_DEVBUILD
		// For now, until we're sure this works on all platforms, we won't block users from starting if they're missing.
		QMessageBox::warning(nullptr, QStringLiteral("Translation Error"),
			QStringLiteral("Failed to find translation file for language '%1':\n%2").arg(language).arg(path));
#endif
		return;
	}

	QTranslator* translator = new QTranslator(qApp);
	if (!translator->load(path))
	{
		QMessageBox::warning(nullptr, QStringLiteral("Translation Error"),
			QStringLiteral("Failed to load translation file for language '%1':\n%2").arg(language).arg(path));
		delete translator;
		return;
	}

	Console.WriteLn(Color_StrongYellow, "Loaded translation file for language %s", language.toUtf8().constData());
	qApp->installTranslator(translator);
	s_translators.push_back(translator);

#ifdef _WIN32
	if (language == QStringLiteral("ja"))
	{
		ImGuiManager::SetFontPath(GetFontPath("msgothic.ttc"));
		ImGuiManager::SetFontRange(GetGlyphRangesJapanese());
	}
	else if (language == QStringLiteral("zh-cn"))
	{
		ImGuiManager::SetFontPath(GetFontPath("msyh.ttc"));
		ImGuiManager::SetFontRange(GetGlyphRangesChinese());
	}
#endif
}

static std::string QtHost::GetFontPath(const char* name)
{
#ifdef _WIN32
	PWSTR folder_path;
	if (FAILED(SHGetKnownFolderPath(FOLDERID_Fonts, 0, nullptr, &folder_path)))
		return fmt::format("C:\\Windows\\Fonts\\%s", name);

	std::string font_path(StringUtil::WideStringToUTF8String(folder_path));
	CoTaskMemFree(folder_path);
	font_path += "\\";
	font_path += name;
	return font_path;
#else
	return name;
#endif
}

std::vector<std::pair<QString, QString>> QtHost::GetAvailableLanguageList()
{
	return {
		{QStringLiteral("English"), QStringLiteral("en")},
	};
}

const char* QtHost::GetDefaultLanguage()
{
	return "en";
}

s32 Host::Internal::GetTranslatedStringImpl(
	const std::string_view& context, const std::string_view& msg, char* tbuf, size_t tbuf_space)
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

static const ImWchar* QtHost::GetGlyphRangesJapanese()
{
	// clang-format off
  // auto update by generate_update_glyph_ranges.py with pcsx2-qt_ja.ts
  static const char16_t chars[] = u"、。あいかがきこさしすずせたってでとなにのはべまみらりるれをんァィイェエカキクグゲシジスセタダチッデトドバパフブプボポミムメモャュョラリルレロンー一中了今件体使信入出制効動取口可変始定実度強後得態択挿易更有本検無状獲用画的索終績能自行覧解設読起送速選開除難面高";
  const int chars_length = sizeof(chars) / sizeof(chars[0]);
	// clang-format on

	static ImWchar base_ranges[] = {
		0x0020, 0x007E, // Basic Latin
	};
	const int base_length = sizeof(base_ranges) / sizeof(base_ranges[0]);

	static ImWchar full_ranges[base_length + chars_length * 2 + 1] = {0};
	memcpy(full_ranges, base_ranges, sizeof(base_ranges));
	for (int i = 0; i < chars_length; i++)
	{
		full_ranges[base_length + i * 2] = full_ranges[base_length + i * 2 + 1] = chars[i];
	}
	return full_ranges;
}

static const ImWchar* QtHost::GetGlyphRangesChinese()
{
	// clang-format off
  // auto update by generate_update_glyph_ranges.py with pcsx2-qt_zh-cn.ts
  static const char16_t chars[] = u"";
  const int chars_length = sizeof(chars) / sizeof(chars[0]);
	// clang-format on

	static ImWchar base_ranges[] = {
		0x0020, 0x007E, // Basic Latin
	};
	const int base_length = sizeof(base_ranges) / sizeof(base_ranges[0]);

	static ImWchar full_ranges[base_length + chars_length * 2 + 1] = {0};
	memcpy(full_ranges, base_ranges, sizeof(base_ranges));
	for (int i = 0; i < chars_length; i++)
	{
		full_ranges[base_length + i * 2] = full_ranges[base_length + i * 2 + 1] = chars[i];
	}
	return full_ranges;
}
