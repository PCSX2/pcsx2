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

#if 0
// Qt internal strings we'd like to have translated
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Services")
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Hide %1")
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Hide Others")
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Show All")
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Preferences...")
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "Quit %1")
QT_TRANSLATE_NOOP("MAC_APPLICATION_MENU", "About %1")
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

static QString getSystemLanguage() {
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
		if (avail == lang) {
			Console.Warning("Couldn't find translation for system language %s, using %s instead",
			                locale.toStdString().c_str(), entry.second.toStdString().c_str());
			return entry.second;
		}
	}
	// No matches :(
	Console.Warning("Couldn't find translation for system language %s, using en instead", locale.toStdString().c_str());
	return QStringLiteral("en");
}

void QtHost::InstallTranslator()
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

	// Install the base qt translation first.
#ifdef __APPLE__
	const QString base_dir = QStringLiteral("%1/../Resources/translations").arg(qApp->applicationDirPath());
#else
	const QString base_dir = QStringLiteral("%1/translations").arg(qApp->applicationDirPath());
#endif
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
			font_path = fmt::format("C:\\Windows\\Fonts\\{}", gi->windows_font_name);
		}
	}
#elif defined(__APPLE__)
	if (gi->mac_font_name)
		font_path = fmt::format("/System/Library/Fonts/{}", gi->mac_font_name);
#endif

	return font_path;
}

const char* QtHost::GetDefaultLanguage()
{
	return "system";
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
		{QCoreApplication::translate("InterfaceSettingsWidget", "System Language [Default]"), QStringLiteral("system")},
		{QStringLiteral("Afrikaans (af-ZA)"), QStringLiteral("af-ZA")},
		{QStringLiteral("عربي (ar-SA)"), QStringLiteral("ar-SA")},
		{QStringLiteral("Català (ca-ES)"), QStringLiteral("ca-ES")},
		{QStringLiteral("Čeština (cs-CZ)"), QStringLiteral("cs-CZ")},
		{QStringLiteral("Dansk (da-DK)"), QStringLiteral("da-DK")},
		{QStringLiteral("Deutsch (de-DE)"), QStringLiteral("de-DE")},
		{QStringLiteral("Ελληνικά (el-GR)"), QStringLiteral("el-GR")},
		{QStringLiteral("English (en)"), QStringLiteral("en")},
		{QStringLiteral("Español (Hispanoamérica) (es-419)"), QStringLiteral("es-419")},
		{QStringLiteral("Español (España) (es-ES)"), QStringLiteral("es-ES")},
		{QStringLiteral("فارسی (fa-IR)"), QStringLiteral("fa-IR")},
		{QStringLiteral("Suomi (fi-FI)"), QStringLiteral("fi-FI")},
		{QStringLiteral("Français (fr-FR)"), QStringLiteral("fr-FR")},
		{QStringLiteral("עִבְרִית (he-IL)"), QStringLiteral("he-IL")},
		{QStringLiteral("Magyar (hu-HU)"), QStringLiteral("hu-HU")},
		{QStringLiteral("Bahasa Indonesia (id-ID)"), QStringLiteral("id-ID")},
		{QStringLiteral("Italiano (it-IT)"), QStringLiteral("it-IT")},
		{QStringLiteral("日本語 (ja-JP)"), QStringLiteral("ja-JP")},
		{QStringLiteral("한국어 (ko-KR)"), QStringLiteral("ko-KR")},
		{QStringLiteral("Latvija (lv-LV)"), QStringLiteral("lv-LV")},
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
		"ja-JP", "msgothic.ttc", nullptr, "ヒラギノ角ゴシック W3.ttc", 
		// auto update by update_glyph_ranges.py with pcsx2-qt_ja-JP.ts
		u"​​……□□△△◯◯✕✕　。々々「』〜〜ああいいううええおせそそたちっぬのばびびぶぶへべほぼまやょろわわをんァイウチッツテニネロワワンン・ー一一上下不与世世両両中中乗乗了了予予事事互互交交今介他他付付代以仮仮件件任任伸伸似似位低体体何何作作使使例例供供係係保保信信修修個個倍倍借借値値停停側側備備像像優優元元先光入入全全公公共共具典内内再再冗冗処処出出分切列列初初判別利利制刷則則削削前前副副割割力力加加効効動動勧勧化化区区十十半半南単印印即即去去参参及及反収取受古古可台右右号号各各合合同名向向否否含含告告周周味味命命品品商商問問善善四四回回囲囲固固国国圧在地地均均垂垂型型域域基基報報場場境境増増壊壊声声変変外外多多大大太太央央失失奇奇奨奨好好妙妙妨妨始始子子字存安安完完定定実実容容寄寄密密対対射射小小少少尾尾展展岐岐崩崩左左巨巨己己帰帰常常幅幅平平序序度座延延式式引引弱弱張張強強当当形形彩彩影影役役待待後後得得御御復復微微心心必必応応忠忠念念急急性性悪悪情情想想意意感感態態慣慣成成戻戻所所手手打打承承投投択択抱抱押押拒拒招招拡拡拳拳持持指指振振挿挿捗捗排排接接推推描提換換揮揮援援揺揺損損撃撃撮撮操操支支改改放放敗敗数数整整文文料料断断新新方方既既日日早早明明時時曲曲更更書書替最有有望望期期未本条条析析果果桁桁案案械械検検極極楽楽概概構構標標権権橙橙機機欄欄欠次止正殊残毎毎比比水水求求汎汎決決況況法法波波注注浪浪浮浮消消深深済済減減渡渡港港湾湾満満源源準準滑滑点為無無照照牲牲特特犠犠状状獲獲率率現現理理璧璧環環生生用用由由申申画画番番異異発登白白的的目目直直相相省省知知短短破破確確示示禁禁秒秒移移程程種種稿稿空空立立端端競競第第算算管管範範精精索索細細終終組組経経結結絞絞統統続続維維緑緒線線編編縦縦縮縮績績繰繰置置義義者者肢肢能能自自致致般般良良色色荷荷落落蔵蔵行行表表衰衰装装製製複複要要見見規規視視覚覚覧覧観観角角解解言言計計記記設設許許訳訳証証試試該詳認認語語誤誤説読調調識識警警象象負負責責費貼質質赤赤起起超超足足跡跡転転軸軸軽軽辞辞込込近近返返追追送送逆逆途途通通速速連連進進遅遅遊遊過過適適選選避避部部配配重量鉄鉄録録長長閉閉開開間間関関閾閾防防降降限限除除陽陽隅隅際際集集離難電電青青非非面面韓韓音音響響頂頂順順領領頻頼題題類類飾飾香香駄駄高高鮮鮮黄黄！！（）／／１２：：＞？～～"
	},
	{
		"ko-KR", "malgun.ttf", nullptr, "AppleSDGothicNeo.ttc",
		// auto update by update_glyph_ranges.py with pcsx2-qt_ko-KR.ts
		u"“”……□□▲△▶▶▼▼◀◀◯◯✕✕んん茶茶가각간간갈갈감값강강같같개개거거건건걸걸검겁것것게게겠겠격격견견결결경경계계고곡공공과과관관교교구국군군권권귀귀규규균균그극근근글글금급긍긍기기긴긴길길깁깁깃깃깅깅깊깊까까깝깝깨깨꺼꺼께께꾸꾸끄끄끔끔끝끝나나날날남남낭낮내내낸낸냅냅너너널널넣네넬넬넷넷노녹높놓누누눈눈눌눌뉴뉴느느는는늘늘능능니닉닌닌님닙닛닛다다단단닫달담담당당대대댑댑더더덜덜덤덤덩덩덮덮데덱덴덴델델뎁뎁도독돌돌동동됐됐되되된된될될됨됩두두듀듀듈듈드득든든들들듬듭등등디디딩딩때때떠떠떤떤떨떨또또뛰뛰뛸뛸뜁뜁뜨뜨라락란란랍랍랑랑래랙랜랜램램랫랫량량러럭런런럴럴럼럽렀렀렇렉렌렌렛렛려력련련렬렬렸령로록롤롤롭롭롯롯료료루루룸룸류류률률르르른른를를름름리릭린린릴릴림립릿릿링링마막만만많많말말맞맞매매맨맨맵맵맺맺머머먼먼멀멀멈멈멋멋메메며며면면명명모목못못몽몽무무문문뮬뮬므므미믹민민밀밀밉밉밍밍및및바바반반받밝밥밥방방배백밴밴뱃뱃버버번번벌벌범법벗벗베벡벤벤벨벨벼벽변변별별병병보복본본볼볼부부분분불불붙붙뷰뷰브브블블비빅빈빈빌빌빙빙빛빛빠빠빨빨사삭산산살살삼삽상상새색샘샘생생샷샷서석선선설설성성세섹센센셀셀셈셉셋셋셔셔션션셰셰소속손손솔솔송송쇄쇄쇠쇠쇼쇼수수순순숨숨쉬쉬슈슈스스슬슬습습시식신신실실심십싱싱싶싶쌍쌍쓰쓰쓸쓸씬씬아아안안않않알알암압았앙애액앤앤앨앨앵앵야약양양어어언언얻얼업없었었에에엔엔여역연연열열영영예예오오온온올올옵옵와와완완왑왑외외왼왼요요용용우우운운울울움웁웃웃워워원원월월웨웨웹웹위위유유율율으으은은을을음음응응의의이이인인일읽임입있있자작잘잘잠잠장장재재잿잿저적전전절절점접정정제제젠젠젤젤젯젯져져조족존존종종좋좌죄죄주주준준줄줄줍줍중중즈즉즌즌즐즐즘즘증증지직진진질질짐집짝짝짧짧째째쪽쪽찌찍차착참참창찾채책챕챕처척천천철철첨첩첫첫청청체체쳐쳐초초촬촬최최추축출출춤춥충충춰춰취취츠측치칙칠칠침칩칭칭카카칸칸칼칼캐캐캔캔캘캘캠캡커커컨컨컬컬컴컴케케켓켓켜켜코코콘콘콜콜콩콩쿳쿳쿼쿼퀀퀀퀴퀴큐큐크크큰큰클클큽큽키키킨킨킬킬킵킵킹킹타타탈탈탐탐태택탠탠탬탭터터턴턴텀텀테텍텐텐텔텔템템토토톱톱통통투투툴툴트특튼튼틀틀티틱팀팀팅팅파파팔팔팝팝패패팻팻퍼퍼페펙편편평평포포폴폴폼폼표표푸푸풀풀품품풍풍퓨퓨프프픈픈플플피픽필필핑핑하하한한할할함합핫핫항항해핵했행향향허허헌헌헤헤현현형형호혹혼혼홈홈홍홍화확환환활활황황회획횟횟횡횡효효후후훨훨휠휠휴휴흐흐흔흔희희히히힌힌"
	},
	{
		"zh-CN", "msyh.ttc", nullptr, "Hiragino Sans GB.ttc",
		// auto update by update_glyph_ranges.py with pcsx2-qt_zh-CN.ts
		u"‘’□□△△○○、。一丁三下不与且且世世丢丢两两个个中中串串为主么义之之乎乎乐乐乘乘也也了了事二于于互互亚些交交产产享享亮亮亲亲人人什什仅仅今今仍从他他代以们们件价任任份份伍伍休休优优会会传传估估伸伸似似但但位住佑佑体体何何余余作作你你佳佳使使例例供供侧侧便便俄俄保保信信修修倍倍借借值值倾倾假假偏偏做做停停储储像像儿儿允允元元充充先光克克免免入入全全六六兰共关关其兹兼兼内内册再写写冲决况况准准减减几几出击函函刀刀刃刃分切列列则则创创删删利利别别到到制刷刹刹前前剪剪副副力力功务动助势势勿勿包包化化匹区十十升升半半协协单单南南占卡卫卫印印即即卸卸压压原原去去参参叉及双反发发取变叠叠口古另另只只可台右右号号各各合吉同后向向吗吗否否含含启启呈呈告告员员味味命命和和哈哈响响哪哪唤唤商商善善器器四四回回因因团团围围国图圈圈在在地地场场址址均均坏坐块块坛坛垂垂型型域域基基堆堆堪堪塔塔填填增增士士声声处处备备复复外外多多够够大大天太失失头头夹夹奇奇奏奏套套奥奥奶奶好好如如始始威威娱娱婴婴媒媒子子字存它它安安完完宏宏官官定定宝实害害家家容容宽宽宿宿寄寄密密富富寸对导导封封射射将将小小少少尔尔尚尚尝尝就就尺尺尼尾局局层层屏屏展展属属崩崩工左巨巨差差己已巴巴希希带帧帮帮常常幅幅幕幕平平并并幸幸序序库底度度延延建建开开异弃弊弊式式引引张张弹强归当录录形形彩彩影影径待很很律律得得循循微微德德心心必忆志志忙忙快快忽忽态态性性怪怪恢恢息息您您悬悬情情惑惑惯惯想想愉愉意意感感慢慢戏我或或战战截截戳戳户户所扁手手才才打打托托执执扩扩扫扬扳扳找找技技抑抑抖抗护护拆拆拉拉拍拍拒拒拟拟拥拦择择括括拳拳持持指指按按挑挑挡挡挪挪振振捕捕损损换换据据捷捷掌掌排排接接控掩描提插插握握搜搜摇摇摘摘撕撕撤撤播播操擎支支收收改改放放故故效效敏敏散散数数整整文文断断斯新方方旋旋无无日日旧旧时时明明易易星映是是显显晕晕普普晰晰暂暂暗暗曲曲更更替最有有服服望望期期未未本本机机权权杆杆束束条条来来板板极极果果柄柄某某染染查查栅栅标栈栏栏校校样根格格框框案案档档桥桥检检棕棕榜榜槽槽模模橙橙次欢欧欧止步死死殊殊段段每每比比毫毫水水求求汇汇没没油油法法波波注注泻泻洲洲活活流流浅浅测测浏浏浪浪浮浮海海消消淡淡深深混混添添清清港港渲渲游游湖湖湾湾溃溃源源溢溢滑滑满满滤滤演演澳澳激激灰灰灵灵点点烈烈热热焦焦然然煞煞照照片版牌牌牙牙特特状状狂狂独独狼狼猎猎猩猩率率王王玩玩环现班班理理瑞瑞甚甚生生用用由由电电画画畅畅界界留留略略疤疤登登白百的的皇皇盖盘目目直直相相省省看看真眠着着知知短短石石码码破破础础硬硬确确碌碌磁磁示示神神禁禁离离种种秒秒积称移移程程稍稍稳稳空空突突窗窗立立站站端端符符第第等等答答筛筛签签简简算算管管类类粉粉粘粘精精糊糊系系素素索索紫紫纠纠红红级级纳纳纹纹线线组组细终经经绑绑结结绕绕绘给络绝统统继继绪绪续续维维绿缀缓缓编编缩缩缺缺网网罗罗置置美美翻翻考考者者而而耗耗耳耳联联肩肩胖胖能能腊腊自自至致舍舍航航良良色色节节芬芬英英范范荐荐荷荷莱莱获获菜菜萄萄著著葡葡蓝蓝藏藏虑虑虚虚融融行行衡衡补补表表被被裁裂装装西西要要覆覆见观规规视视览觉角角解解触触言言警警计计认认让让议议记记许许论论设访证证识识诊诊译译试试询询该详语语误误说说请诸读读调调谍谍豹豹负负败账质质贴贴费费赛赛赫赫起起超超越越足足跃跃跟跟跨跨路路跳跳踏踏踪踪身身车车轨轨转转轮软轴轴轻轻载载较较辅辅辑辑输输辨辨边边达达过过迎迎运近还这进进连迟述述追追退适逆逆选选逐逐递递通通速造遇遇道道避避那那邻邻部部都都配配醒醒采采释释里重量量针针钮钮铁铁铺铺链链销锁锐锐错错键锯镜镜长长门门闭问闲闲间间阅阅队队防防阴阴附陆降降限限除除险险随隐隔隔隙隙障障雄雄集集零零雾雾需需震震静静非非靠靠面面韩韩音音顶顶项须顿顿预预频频题题颜额风风饱饱馈馈首首香香马马驱驱骤骤高高鬼鬼魂魂魔魔麦麦黄黄黑黑默默鼓鼓鼠鼠齐齐齿齿，，：：？？"
	},
	{
		"zh-TW", "msyh.ttc", nullptr, "Hiragino Sans GB.ttc",
		// auto update by update_glyph_ranges.py with pcsx2-qt_zh-TW.ts
		u"、。一一上下不不且且並並中中主主了了互互今今令以件件任任位位何何使使來來係係保保個個優優儲儲免免入入內全具具兼兼冊冊再再到到創創功加助助動動務務勢勢包包協協南南及及取取只只可可合合名名和和商商問問啟啟嘗嘗器器嚮嚮在在壇壇多多失失如如始始娛娛存存它它安安完完官官定定容容密密將將對導就就尼尼局局希希幫幫序序庫庫建建後後得得從從您您情情意意態態應應成成戲戲戶戶所所持持指指接接擇擇擬擬支支改改敗敗新新方方於於是是時時更更會會有有服服望望本本果果查查樂樂標標模模機機檢檢次次歡步沒沒況況消消源源為為然然版版牌牌狀狀獲獲玩玩現現理理生生用用界界登發的的看看硬硬確確碼碼程程種種站站第第管管系系索索組組絡絡統統網網緒緒編編置置而而聯聯能能自自與與虛虛裝裝要要解解言言訪訪設設許許註註試試該該認認語語誤誤請請論論譯議費費起起載載輸輸迎迎這通連連遊遊過過選選配配釋釋重重錄錄錯錯鏈鏈開開間間關關防防附附需需面面項項題題驟驟默默"
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
