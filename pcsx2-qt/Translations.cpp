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
		{QStringLiteral("简体中文"), QStringLiteral("zh-cn")},
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
  static const char16_t chars[] = u"‘’□△○、。一丁三上下不与且世丢两个中丰串为主么义之乎乐乘也了事二于互亚些交产享亮亲人什仅介仍从他代令以们件价任份伍休优会传估伸似但位低住佑体何余作佣佳使例供侧便俄保信修倍值倾假偏做停储像儿允元充先光克免入全六兰共关兵其具典兼内册再写冲决况净准减几出击函刀刃分切列则创删利别到制刷刹前剪副力功加务动助势勿包化匹区十升半协单南占卡卫印即压原去参及双反发取受变叠口古另只可台右号各合吉同名后向吗否含启呈告员味命和哈响哪唤商善器四回因团围国图圈在地场址均坏坐块坛垂型域基堆堪塔填增士声处备复外多够大天太失头夹奇奏套奥奶好如始威娱婴媒子字存它安完宏官定宝实害家容宽宿寄密富寸对导封射将小少尔尚尝就尺尼尽尾局层屏展属崩工左巨差己已巴希带帧帮常幅幕平并幸序库应底度延建开异弃弊式引张弹强归当录形彩影径待很得循微德心必忆志忙快忽态性怪恢息您悬情惑惯意感慢戏成我或战截戳户所扁手打托执扩扫扬扳找技抑抖抗护拆拉拍拒拟拦择括拳持指按挑挡挪振捕损换据掌排接控推掩描提插握搜摇撕撤播操擎支收改放故效敏散数整文断斯新方旋无日旧时明易星映是显晕普晰暂暗曲更替最有服期未本机权杆束条来板极果柄某染查栅标栈栏校样核根格框案档桥检棕榜槽模橙次欧止正此步死殊段每比毫水求汇没油法波注泻洲活流浅测浏浪浮海消深混添清港渲游湖湾溃源溢滑满滤演澳激灰灵点烈热焦然煞照片版牌牙特状狂独狼猎猩率王玩环现班理瑞甚生用由甲电画畅界留略疤登白百的皇盖盗盘目直相省看真眠着知短石码破础硬确碌磁示神禁离种秒积称移程稍稳空突窗立站端符第等筛签简算管类粉粘精糊系素索紫纠红级纯纳纹线组细织终经绑结绕绘给络绝统继绪续维绿缀缓编缩网罗置美翻考者而耗耳联肩胖能腊自至致舍良色节芬英范荐荷莱获菜萄著葡蓝藏虑虚融行衡补表被裁裂装西要覆观规视览觉角解触言警计认让议记许论设访证诊译试询该详语误说请诸读调谍豹负败账质贴费赛起超越足跟跨路跳踏踪身车轨转轮软轴轻载较辅辑输辨边达过运近还这进连迟述追退送适选逐递通速造遇道避那部都配醒采释里重量针钮铁铺链销锁锐错键锯镜长门闭问闲间队防阴附陆降限除险随隐隔隙障雄集零雾需震静非靠面韩音顶项顺须顿预频题颜额风饱馈首香马驱验高鬼魂魔麦黄黑默鼓鼠齐齿，？";
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
