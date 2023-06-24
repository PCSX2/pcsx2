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

#include "common/Assertions.h"
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
	struct GlyphInfo
	{
		const char* language;
		const char* windows_font_name;
		const char* linux_font_name;
		const char* mac_font_name;
		const char16_t* used_glyphs;
	};

	static std::string GetFontPath(const GlyphInfo* gi);
	static void UpdateGlyphRanges(const std::string_view& language);
	static const GlyphInfo* GetGlyphInfo(const std::string_view& language);

	static std::vector<ImWchar> s_glyph_ranges;
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

	UpdateGlyphRanges(language.toStdString());

	// Clear translation cache after installing translators, to prevent races.
	Host::ClearTranslationCache();
}

static std::string QtHost::GetFontPath(const GlyphInfo* gi)
{
	std::string font_path;

#ifdef _WIN32
	if (gi->windows_font_name)
	{
		PWSTR folder_path;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Fonts, 0, nullptr, &folder_path)))
		{
			font_path = StringUtil::WideStringToUTF8String(folder_path);
			CoTaskMemFree(folder_path);
			font_path += "\\";
			font_path += gi->windows_font_name;
		}
		else
		{
			font_path = fmt::format("C:\\Windows\\Fonts\\%s", gi->windows_font_name);
		}
	}
#endif

	return font_path;
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

std::vector<std::pair<QString, QString>> QtHost::GetAvailableLanguageList()
{
	return {
		{QStringLiteral("Afrikaans (af-ZA)"), QStringLiteral("af-ZA")},
		{QStringLiteral("عربي (ar-SA)"), QStringLiteral("ar-SA")},
		{QStringLiteral("Català (ca-ES)"), QStringLiteral("ca-ES")},
		{QStringLiteral("Čeština (cs-CZ)"), QStringLiteral("cs-CZ")},
		{QStringLiteral("Dansk (da-DK)"), QStringLiteral("da-DK")},
		{QStringLiteral("Deutsch (de-DE)"), QStringLiteral("de-DE")},
		{QStringLiteral("Ελληνικά (el-GR)"), QStringLiteral("el-GR")},
		{QStringLiteral("English (en)"), QStringLiteral("en")},
		{QStringLiteral("Español (Latin America) (es-419)"), QStringLiteral("es-419")},
		{QStringLiteral("Español (es-ES)"), QStringLiteral("es-ES")},
		{QStringLiteral("فارسی (fa-IR)"), QStringLiteral("fa-IR")},
		{QStringLiteral("Suomi (fi-FI)"), QStringLiteral("fi-FI")},
		{QStringLiteral("Français (fr-FR)"), QStringLiteral("fr-FR")},
		{QStringLiteral("עִבְרִית (he-IL)"), QStringLiteral("he-IL")},
		{QStringLiteral("Magyar (hu-HU)"), QStringLiteral("hu-HU")},
		{QStringLiteral("Bahasa Indonesia (in-ID)"), QStringLiteral("id-ID")},
		{QStringLiteral("Italiano (it-IT)"), QStringLiteral("it-IT")},
		{QStringLiteral("日本語 (ja-JP)"), QStringLiteral("ja-JP")},
		{QStringLiteral("한국어 (ko-KR)"), QStringLiteral("ko-KR")},
		{QStringLiteral("Nederlands (nl-NL)"), QStringLiteral("nl-NL")},
		{QStringLiteral("Norsk (nl-NL)"), QStringLiteral("no-NO")},
		{QStringLiteral("Polski (pl-PL)"), QStringLiteral("pl-PL")},
		{QStringLiteral("Português (Br) (pt-BR)"), QStringLiteral("pt-BR")},
		{QStringLiteral("Português (Pt) (pt-PT)"), QStringLiteral("pt-PT")},
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

static constexpr const ImWchar s_base_latin_range[] = {
	0x0020, 0x00FF, // Basic Latin + Latin Supplement
};
static constexpr const ImWchar s_central_european_ranges[] = {
	0x0100, 0x017F, // Central European diacritics
};

void QtHost::UpdateGlyphRanges(const std::string_view& language)
{
	const GlyphInfo* gi = GetGlyphInfo(language);

	std::string font_path;
	s_glyph_ranges.clear();

	// Base Latin range is always included.
	s_glyph_ranges.insert(s_glyph_ranges.begin(), std::begin(s_base_latin_range), std::end(s_base_latin_range));

	if (gi)
	{
		if (gi->used_glyphs)
		{
			const char16_t* ptr = gi->used_glyphs;
			while (*ptr != 0)
			{
				// Always should be in pairs.
				pxAssert(ptr[0] != 0 && ptr[1] != 0);
				s_glyph_ranges.push_back(*(ptr++));
				s_glyph_ranges.push_back(*(ptr++));
			}
		}

		font_path = GetFontPath(gi);
	}

	// If we don't have any specific glyph range, assume Central European, except if English, then keep the size down.
	if ((!gi || !gi->used_glyphs) && language != "en")
	{
		s_glyph_ranges.insert(
			s_glyph_ranges.begin(), std::begin(s_central_european_ranges), std::end(s_central_european_ranges));
	}

	// List terminator.
	s_glyph_ranges.push_back(0);
	s_glyph_ranges.push_back(0);

	ImGuiManager::SetFontPath(std::move(font_path));
	ImGuiManager::SetFontRange(s_glyph_ranges.data());
}

// clang-format off
static constexpr const char16_t s_cyrillic_ranges[] = {
	/* Cyrillic + Cyrillic Supplement */ 0x0400, 0x052F, /* Extended-A */ 0x2DE0, 0x2DFF, /* Extended-B */ 0xA640, 0xA69F, 0, 0
};
static constexpr const QtHost::GlyphInfo s_glyph_info[] = {
	// Cyrillic languages
	{ "ru-RU", nullptr, nullptr, nullptr, s_cyrillic_ranges },
	{ "sr-SP", nullptr, nullptr, nullptr, s_cyrillic_ranges },
	{ "uk-UA", nullptr, nullptr, nullptr, s_cyrillic_ranges },

	{
		"ja-JP", "msgothic.ttc", nullptr, nullptr,
		// auto update by update_glyph_ranges.py with pcsx2-qt_ja-JP.ts
		u"​​□□△△◯◯✕✕、。〜〜ああいいううええおきくぐここささしすせせそそただっつてにのはびびへべほほまみめもややよれわわをんァイウコサソタチッツテニネパビロワワンンーー一一上下不不中中丸丸了了互互今今他他代代仮仮作作使使保保信信修修倍倍値値停停備備像像入入全全公公内内再再出出切切別別利利制制削削副副割割力力加加効効動動勧勧化化十十南南参参反収取取古古可台右右合合同名向向回回固固国国在在報報場場境境壊壊変変外外大大失失奨奨始始字存完完定定実実左左帰帰常常幅幅度度式式張張形形待待後後御御性性情情想想意意感感態態成成投投択択拡拡推推提提換換敗敗数数整整新新方方既既日日明明時時更更書書替最有有期期本本果果検検標標橙橙機機止正毎毎比比況況注注消消港港湾湾準準無無照照状状率率現現璧璧環環生生用用画画異異的的直直知知確確示示種種稿稿空空索索終終緑緑編編績績能能自自般般行行表表製製複複視視覧覧解解言言設設認認語語読読赤赤起起跡跡転転軸軸込込近近追追送送通通速速進進遅遅遊遊適適選選部部長長閉閉開開関関防防限限除除隅隅集集青青非非面面韓韓音音類類香香高高黄黄＋＋？？"
	},
	{
		"ko-KR", "malgun.ttf", nullptr, nullptr,
		// auto update by update_glyph_ranges.py with pcsx2-qt_ko-KR.ts
		u""
	},
	{
		"zh-CN", "msyh.ttc", nullptr, nullptr,
		// auto update by update_glyph_ranges.py with pcsx2-qt_zh-CN.ts
		u"‘’□□△△○○、。一丁三下不与且且世世丢丢两两个个中中丰丰串串为主么义之之乎乎乐乐乘乘也也了了事二于于互互亚些交交产产享享亮亮亲亲人人什什仅仅介介仍从他他代以们们件价任任份份伍伍休休优优会会传传估估伸伸似似但但位住佑佑体体何何余余作作佣佣佳佳使使例例供供侧侧便便俄俄保保信信修修倍倍值值倾倾假假偏偏做做停停储储像像儿儿允允元元充充先光克克免免入入全全六六兰共关关兵典兼兼内内册再写写冲决况况净净准准减减几几出击函函刀刀刃刃分切列列则则创创删删利利别别到到制刷刹刹前前剪剪副副力力功务动助势势勿勿包包化化匹区十十升升半半协协单单南南占卡卫卫印印即即压压原原去去参参及及双反发发取变叠叠口古另另只只可台右右号号各各合吉同后向向吗吗否否含含启启呈呈告告员员味味命命和和哈哈响响哪哪唤唤商商善善器器四四回回因因团团围围国图圈圈在在地地场场址址均均坏坐块块坛坛垂垂型型域域基基堆堆堪堪塔塔填填增增士士声声处处备备复复外外多多够够大大天太失失头头夹夹奇奇奏奏套套奥奥奶奶好好如如始始威威娱娱婴婴媒媒子子字存它它安安完完宏宏官官定定宝实害害家家容容宽宽宿宿寄寄密密富富寸对导导封封射射将将小小少少尔尔尚尚尝尝就就尺尺尼尾局局层层屏屏展展属属崩崩工左巨巨差差己已巴巴希希带帧帮帮常常幅幅幕幕平平并并幸幸序序库底度度延延建建开开异弃弊弊式式引引张张弹强归当录录形形彩彩影影径待很很得得循循微微德德心心必忆志志忙忙快快忽忽态态性性怪怪恢恢息息您您悬悬情情惑惑惯惯意意感感慢慢戏我或或战战截截戳戳户户所扁手手打打托托执执扩扩扫扬扳扳找找技技抑抑抖抗护护拆拆拉拉拍拍拒拒拟拟拦拦择择括括拳拳持持指指按按挑挑挡挡挪挪振振捕捕损损换换据据掌掌排排接接控掩描提插插握握搜搜摇摇撕撕撤撤播播操擎支支收收改改放放故故效效敏敏散散数数整整文文断断斯新方方旋旋无无日日旧旧时时明明易易星映是是显显晕晕普普晰晰暂暂暗暗曲曲更更替最有有服服期期未未本本机机权权杆杆束束条条来来板板极极果果柄柄某某染染查查栅栅标栈栏栏校校样根格格框框案案档档桥桥检检棕棕榜榜模模橙橙次次欧欧止步死死殊殊段段每每比比毫毫水水求求汇汇没没油油法法波波注注泻泻洲洲活活流流浅浅测测浏浏浪浪浮浮海海消消深深混混添添清清港港渲渲游游湖湖湾湾溃溃源源溢溢滑滑满满滤滤演演澳澳激激灰灰灵灵点点烈烈热热焦焦然然煞煞照照片版牌牌牙牙特特状状狂狂独独狼狼猎猎猩猩率率王王玩玩环现班班理理瑞瑞甚甚生生用用由甲电电画画畅畅界界留留略略疤疤登登白百的的皇皇盖盘目目直直相相省省看看真眠着着知知短短石石码码破破础础硬硬确确碌碌磁磁示示神神禁禁离离种种秒秒积称移移程程稍稍稳稳空空突突窗窗立立站站端端符符第第等等筛筛签签简简算算管管类类粉粉粘粘精精糊糊系系素素索索紫紫纠纠红红级级纯纯纳纳纹纹线线组组细终经经绑绑结结绕绕绘给络绝统统继继绪绪续续维维绿缀缓缓编编缩缩网网罗罗置置美美翻翻考考者者而而耗耗耳耳联联肩肩胖胖能能腊腊自自至致舍舍良良色色节节芬芬英英范范荐荐荷荷莱莱获获菜菜萄萄著著葡葡蓝蓝藏藏虑虑虚虚融融行行衡衡补补表表被被裁裂装装西西要要覆覆观观规规视视览觉角角解解触触言言警警计计认认让让议议记记许许论论设访证证诊诊译译试试询询该详语语误误说说请诸读读调调谍谍豹豹负负败账质质贴贴费费赛赛起起超超越越足足跟跟跨跨路路跳跳踏踏踪踪身身车车轨轨转转轮软轴轴轻轻载载较较辅辅辑辑输输辨辨边边达达过过运近还这进进连迟述述追追退适选选逐逐递递通通速造遇遇道道避避那那部部都都配配醒醒采采释释里重量量针针钮钮铁铁铺铺链链销锁锐锐错错键锯镜镜长长门门闭问闲闲间间队队防防阴阴附附陆陆降降限限除除险险随隐隔隔隙隙障障雄雄集集零零雾雾需需震震静静非非靠靠面面韩韩音音顶顶项须顿顿预预频频题题颜额风风饱饱馈馈首首香香马马驱驱验验高高鬼鬼魂魂魔魔麦麦黄黄黑黑默默鼓鼓鼠鼠齐齐齿齿，，？？"
	},
	{
		"zh-TW", "msyh.ttc", nullptr, nullptr,
		// auto update by update_glyph_ranges.py with pcsx2-qt_zh-TW.ts
		u""
	},
};
// clang-format on

const QtHost::GlyphInfo* QtHost::GetGlyphInfo(const std::string_view& language)
{
	for (const GlyphInfo& it : s_glyph_info)
	{
		if (language == it.language)
			return &it;
	}

	return nullptr;
}
