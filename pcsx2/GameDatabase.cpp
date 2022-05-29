/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "GameDatabase.h"
#include "Host.h"
#include "Patch.h"
#include "vtlb.h"

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include <sstream>
#include "ryml_std.hpp"
#include "ryml.hpp"
#include "fmt/core.h"
#include "fmt/ranges.h"
#include <fstream>
#include <mutex>
#include <optional>

namespace GameDatabaseSchema
{
	static const char* getHWFixName(GSHWFixId id);
	static std::optional<GSHWFixId> parseHWFixName(const std::string_view& name);
	static bool isUserHackHWFix(GSHWFixId id);
} // namespace GameDatabaseSchema

namespace GameDatabase
{
	static void parseAndInsert(const std::string_view& serial, const c4::yml::NodeRef& node);
	static void initDatabase();
} // namespace GameDatabase

static constexpr char GAMEDB_YAML_FILE_NAME[] = "GameIndex.yaml";

static std::unordered_map<std::string, GameDatabaseSchema::GameEntry> s_game_db;
static std::once_flag s_load_once_flag;

std::string GameDatabaseSchema::GameEntry::memcardFiltersAsString() const
{
	return fmt::to_string(fmt::join(memcardFilters, "/"));
}

const std::string* GameDatabaseSchema::GameEntry::findPatch(u32 crc) const
{
	Console.WriteLn(fmt::format("[GameDB] Searching for patch with CRC '{:08X}'", crc));

	auto it = patches.find(crc);
	if (it != patches.end())
	{
		Console.WriteLn(fmt::format("[GameDB] Found patch with CRC '{:08X}'", crc));
		return &it->second;
	}

	it = patches.find(0);
	if (it != patches.end())
	{
		Console.WriteLn("[GameDB] Found and falling back to default patch");
		return &it->second;
	}
	Console.WriteLn("[GameDB] No CRC-specific patch or default patch found");
	return nullptr;
}

const char* GameDatabaseSchema::GameEntry::compatAsString() const
{
	switch (compat)
	{
		case GameDatabaseSchema::Compatibility::Perfect:
			return "Perfect";
		case GameDatabaseSchema::Compatibility::Playable:
			return "Playable";
		case GameDatabaseSchema::Compatibility::InGame:
			return "In-Game";
		case GameDatabaseSchema::Compatibility::Menu:
			return "Menu";
		case GameDatabaseSchema::Compatibility::Intro:
			return "Intro";
		case GameDatabaseSchema::Compatibility::Nothing:
			return "Nothing";
		default:
			return "Unknown";
	}
}

void GameDatabase::parseAndInsert(const std::string_view& serial, const c4::yml::NodeRef& node)
{
	GameDatabaseSchema::GameEntry gameEntry;
	if (node.has_child("name"))
	{
		node["name"] >> gameEntry.name;
	}
	if (node.has_child("region"))
	{
		node["region"] >> gameEntry.region;
	}
	if (node.has_child("compat"))
	{
		int val = 0;
		node["compat"] >> val;
		gameEntry.compat = static_cast<GameDatabaseSchema::Compatibility>(val);
	}
	if (node.has_child("roundModes"))
	{
		if (node["roundModes"].has_child("eeRoundMode"))
		{
			int eeVal = -1;
			node["roundModes"]["eeRoundMode"] >> eeVal;
			gameEntry.eeRoundMode = static_cast<GameDatabaseSchema::RoundMode>(eeVal);
		}
		if (node["roundModes"].has_child("vuRoundMode"))
		{
			int vuVal = -1;
			node["roundModes"]["vuRoundMode"] >> vuVal;
			gameEntry.vuRoundMode = static_cast<GameDatabaseSchema::RoundMode>(vuVal);
		}
	}
	if (node.has_child("clampModes"))
	{
		if (node["clampModes"].has_child("eeClampMode"))
		{
			int eeVal = -1;
			node["clampModes"]["eeClampMode"] >> eeVal;
			gameEntry.eeClampMode = static_cast<GameDatabaseSchema::ClampMode>(eeVal);
		}
		if (node["clampModes"].has_child("vuClampMode"))
		{
			int vuVal = -1;
			node["clampModes"]["vuClampMode"] >> vuVal;
			gameEntry.vuClampMode = static_cast<GameDatabaseSchema::ClampMode>(vuVal);
		}
	}

	// Validate game fixes, invalid ones will be dropped!
	if (node.has_child("gameFixes") && node["gameFixes"].has_children())
	{
		for (const ryml::NodeRef& n : node["gameFixes"].children())
		{
			bool fixValidated = false;
			auto fix = std::string(n.val().str, n.val().len);

			// Enum values don't end with Hack, but gamedb does, so remove it before comparing.
			if (StringUtil::EndsWith(fix, "Hack"))
			{
				fix.erase(fix.size() - 4);
				for (GamefixId id = GamefixId_FIRST; id < pxEnumEnd; id++)
				{
					if (fix.compare(EnumToString(id)) == 0 &&
						std::find(gameEntry.gameFixes.begin(), gameEntry.gameFixes.end(), id) == gameEntry.gameFixes.end())
					{
						gameEntry.gameFixes.push_back(id);
						fixValidated = true;
						break;
					}
				}
			}

			if (!fixValidated)
			{
				Console.Error(fmt::format("[GameDB] Invalid gamefix: '{}', specified for serial: '{}'. Dropping!", fix, serial));
			}
		}
	}

	// Validate speed hacks, invalid ones will be dropped!
	if (node.has_child("speedHacks") && node["speedHacks"].has_children())
	{
		for (const ryml::NodeRef& n : node["speedHacks"].children())
		{
			bool speedHackValidated = false;
			auto speedHack = std::string(n.key().str, n.key().len);

			// Same deal with SpeedHacks
			if (StringUtil::EndsWith(speedHack, "SpeedHack"))
			{
				speedHack.erase(speedHack.size() - 9);
				for (SpeedhackId id = SpeedhackId_FIRST; id < pxEnumEnd; id++)
				{
					if (speedHack.compare(EnumToString(id)) == 0 &&
						std::none_of(gameEntry.speedHacks.begin(), gameEntry.speedHacks.end(), [id](const auto& it) { return it.first == id; }))
					{
						gameEntry.speedHacks.emplace_back(id, std::atoi(n.val().str));
						speedHackValidated = true;
						break;
					}
				}
			}

			if (!speedHackValidated)
			{
				Console.Error(fmt::format("[GameDB] Invalid speedhack: '{}', specified for serial: '{}'. Dropping!", speedHack.c_str(), serial));
			}
		}
	}

	if (node.has_child("gsHWFixes"))
	{
		for (const ryml::NodeRef& n : node["gsHWFixes"].children())
		{
			const std::string_view id_name(n.key().data(), n.key().size());
			std::optional<GameDatabaseSchema::GSHWFixId> id = GameDatabaseSchema::parseHWFixName(id_name);
			std::optional<s32> value = n.has_val() ? StringUtil::FromChars<s32>(std::string_view(n.val().data(), n.val().size())) : 1;
			if (!id.has_value() || !value.has_value())
			{
				Console.Error("[GameDB] Invalid GS HW Fix: '%.*s' specified for serial '%.*s'. Dropping!",
					static_cast<int>(id_name.size()), id_name.data(),
					static_cast<int>(serial.size()), serial.data());
				continue;
			}

			gameEntry.gsHWFixes.emplace_back(id.value(), value.value());
		}
	}

	// Memory Card Filters - Store as a vector to allow flexibility in the future
	// - currently they are used as a '\n' delimited string in the app
	if (node.has_child("memcardFilters") && node["memcardFilters"].has_children())
	{
		for (const ryml::NodeRef& n : node["memcardFilters"].children())
		{
			auto memcardFilter = std::string(n.val().str, n.val().len);
			gameEntry.memcardFilters.emplace_back(std::move(memcardFilter));
		}
	}

	// Game Patches
	if (node.has_child("patches") && node["patches"].has_children())
	{
		for (const ryml::NodeRef& n : node["patches"].children())
		{
			// use a crc of 0 for default patches
			const std::string_view crc_str(n.key().str, n.key().len);
			const std::optional<u32> crc = (StringUtil::compareNoCase(crc_str, "default")) ? std::optional<u32>(0) : StringUtil::FromChars<u32>(crc_str, 16);
			if (!crc.has_value())
			{
				Console.Error(fmt::format("[GameDB] Invalid CRC '{}' found for serial: '{}'. Skipping!", crc_str, serial));
				continue;
			}
			if (gameEntry.patches.find(crc.value()) != gameEntry.patches.end())
			{
				Console.Error(fmt::format("[GameDB] Duplicate CRC '{}' found for serial: '{}'. Skipping, CRCs are case-insensitive!", crc_str, serial));
				continue;
			}

			std::string patch;
			if (n.has_child("content"))
				n["content"] >> patch;
			gameEntry.patches.emplace(crc.value(), std::move(patch));
		}
	}

	s_game_db.emplace(std::move(serial), std::move(gameEntry));
}

static const char* s_gs_hw_fix_names[] = {
	"autoFlush",
	"conservativeFramebuffer",
	"cpuFramebufferConversion",
	"disableDepthSupport",
	"wrapGSMem",
	"preloadFrameData",
	"disablePartialInvalidation",
	"textureInsideRT",
	"alignSprite",
	"mergeSprite",
	"wildArmsHack",
	"pointListPalette",
	"mipmap",
	"trilinearFiltering",
	"skipDrawStart",
	"skipDrawEnd",
	"halfBottomOverride",
	"halfPixelOffset",
	"roundSprite",
	"texturePreloading",
	"deinterlace",
};
static_assert(std::size(s_gs_hw_fix_names) == static_cast<u32>(GameDatabaseSchema::GSHWFixId::Count), "HW fix name lookup is correct size");

const char* GameDatabaseSchema::getHWFixName(GSHWFixId id)
{
	return s_gs_hw_fix_names[static_cast<u32>(id)];
}

static std::optional<GameDatabaseSchema::GSHWFixId> GameDatabaseSchema::parseHWFixName(const std::string_view& name)
{
	for (u32 i = 0; i < std::size(s_gs_hw_fix_names); i++)
	{
		if (name.compare(s_gs_hw_fix_names[i]) == 0)
			return static_cast<GameDatabaseSchema::GSHWFixId>(i);
	}

	return std::nullopt;
}

bool GameDatabaseSchema::isUserHackHWFix(GSHWFixId id)
{
	switch (id)
	{
		case GSHWFixId::Deinterlace:
		case GSHWFixId::Mipmap:
		case GSHWFixId::TexturePreloading:
		case GSHWFixId::ConservativeFramebuffer:
		case GSHWFixId::PointListPalette:
			return false;

#ifdef PCSX2_CORE
			// Trifiltering isn't a hack in Qt.
		case GSHWFixId::TrilinearFiltering:
			return false;
#endif

		default:
			return true;
	}
}

u32 GameDatabaseSchema::GameEntry::applyGameFixes(Pcsx2Config& config, bool applyAuto) const
{
	// Only apply core game fixes if the user has enabled them.
	if (!applyAuto)
		Console.Warning("[GameDB] Game Fixes are disabled");

	u32 num_applied_fixes = 0;

	if (eeRoundMode != GameDatabaseSchema::RoundMode::Undefined)
	{
		const SSE_RoundMode eeRM = (SSE_RoundMode)enum_cast(eeRoundMode);
		if (EnumIsValid(eeRM))
		{
			if (applyAuto)
			{
				PatchesCon->WriteLn("(GameDB) Changing EE/FPU roundmode to %d [%s]", eeRM, EnumToString(eeRM));
				config.Cpu.sseMXCSR.SetRoundMode(eeRM);
				num_applied_fixes++;
			}
			else
				PatchesCon->Warning("[GameDB] Skipping changing EE/FPU roundmode to %d [%s]", eeRM, EnumToString(eeRM));
		}
	}

	if (vuRoundMode != GameDatabaseSchema::RoundMode::Undefined)
	{
		const SSE_RoundMode vuRM = (SSE_RoundMode)enum_cast(vuRoundMode);
		if (EnumIsValid(vuRM))
		{
			if (applyAuto)
			{
				PatchesCon->WriteLn("(GameDB) Changing VU0/VU1 roundmode to %d [%s]", vuRM, EnumToString(vuRM));
				config.Cpu.sseVUMXCSR.SetRoundMode(vuRM);
				num_applied_fixes++;
			}
			else
				PatchesCon->Warning("[GameDB] Skipping changing VU0/VU1 roundmode to %d [%s]", vuRM, EnumToString(vuRM));
		}
	}

	if (eeClampMode != GameDatabaseSchema::ClampMode::Undefined)
	{
		const int clampMode = enum_cast(eeClampMode);
		if (applyAuto)
		{
			PatchesCon->WriteLn("(GameDB) Changing EE/FPU clamp mode [mode=%d]", clampMode);
			config.Cpu.Recompiler.fpuOverflow = (clampMode >= 1);
			config.Cpu.Recompiler.fpuExtraOverflow = (clampMode >= 2);
			config.Cpu.Recompiler.fpuFullMode = (clampMode >= 3);
			num_applied_fixes++;
		}
		else
			PatchesCon->Warning("[GameDB] Skipping changing EE/FPU clamp mode [mode=%d]", clampMode);
	}

	if (vuClampMode != GameDatabaseSchema::ClampMode::Undefined)
	{
		const int clampMode = enum_cast(vuClampMode);
		if (applyAuto)
		{
			PatchesCon->WriteLn("(GameDB) Changing VU0/VU1 clamp mode [mode=%d]", clampMode);
			config.Cpu.Recompiler.vuOverflow = (clampMode >= 1);
			config.Cpu.Recompiler.vuExtraOverflow = (clampMode >= 2);
			config.Cpu.Recompiler.vuSignOverflow = (clampMode >= 3);
			num_applied_fixes++;
		}
		else
			PatchesCon->Warning("[GameDB] Skipping changing VU0/VU1 clamp mode [mode=%d]", clampMode);
	}

	// TODO - config - this could be simplified with maps instead of bitfields and enums
	for (const auto& it : speedHacks)
	{
		const bool mode = it.second != 0;
		if (!applyAuto)
		{
			PatchesCon->Warning("[GameDB] Skipping setting Speedhack '%s' to [mode=%d]", EnumToString(it.first), mode);
			continue;
		}
		// Legacy note - speedhacks are setup in the GameDB as integer values, but
		// are effectively booleans like the gamefixes
		config.Speedhacks.Set(it.first, mode);
		PatchesCon->WriteLn("(GameDB) Setting Speedhack '%s' to [mode=%d]", EnumToString(it.first), mode);
		num_applied_fixes++;
	}

	// TODO - config - this could be simplified with maps instead of bitfields and enums
	for (const GamefixId id : gameFixes)
	{
		if (!applyAuto)
		{
			PatchesCon->Warning("[GameDB] Skipping Gamefix: %s", EnumToString(id));
			continue;
		}
		// if the fix is present, it is said to be enabled
		config.Gamefixes.Set(id, true);
		PatchesCon->WriteLn("(GameDB) Enabled Gamefix: %s", EnumToString(id));
		num_applied_fixes++;

		// The LUT is only used for 1 game so we allocate it only when the gamefix is enabled (save 4MB)
		if (id == Fix_GoemonTlbMiss && true)
			vtlb_Alloc_Ppmap();
	}

	return num_applied_fixes;
}

u32 GameDatabaseSchema::GameEntry::applyGSHardwareFixes(Pcsx2Config::GSOptions& config) const
{
	// Only apply GS HW fixes if the user hasn't manually enabled HW fixes.
	const bool apply_auto_fixes = !config.ManualUserHacks;
	if (!apply_auto_fixes)
		Console.Warning("[GameDB] Manual GS hardware renderer fixes are enabled, not using automatic hardware renderer fixes from GameDB.");

	u32 num_applied_fixes = 0;
	for (const auto& [id, value] : gsHWFixes)
	{
		if (isUserHackHWFix(id) && !apply_auto_fixes)
		{
			PatchesCon->Warning("[GameDB] Skipping GS Hardware Fix: %s to [mode=%d]", getHWFixName(id), value);
			continue;
		}

		switch (id)
		{
			case GSHWFixId::AutoFlush:
				config.UserHacks_AutoFlush = (value > 0);
				break;

			case GSHWFixId::ConservativeFramebuffer:
				config.ConservativeFramebuffer = (value > 0);
				break;

			case GSHWFixId::CPUFramebufferConversion:
				config.UserHacks_CPUFBConversion = (value > 0);
				break;

			case GSHWFixId::DisableDepthSupport:
				config.UserHacks_DisableDepthSupport = (value > 0);
				break;

			case GSHWFixId::WrapGSMem:
				config.WrapGSMem = (value > 0);
				break;

			case GSHWFixId::PreloadFrameData:
				config.PreloadFrameWithGSData = (value > 0);
				break;

			case GSHWFixId::DisablePartialInvalidation:
				config.UserHacks_DisablePartialInvalidation = (value > 0);
				break;

			case GSHWFixId::TextureInsideRT:
				config.UserHacks_TextureInsideRt = (value > 0);
				break;

			case GSHWFixId::AlignSprite:
				config.UserHacks_AlignSpriteX = (value > 0);
				break;

			case GSHWFixId::MergeSprite:
				config.UserHacks_MergePPSprite = (value > 0);
				break;

			case GSHWFixId::WildArmsHack:
				config.UserHacks_WildHack = (value > 0);
				break;

			case GSHWFixId::PointListPalette:
				config.PointListPalette = (value > 0);
				break;

			case GSHWFixId::Mipmap:
			{
				if (value >= 0 && value <= static_cast<int>(HWMipmapLevel::Full))
				{
					if (config.HWMipmap == HWMipmapLevel::Automatic)
						config.HWMipmap = static_cast<HWMipmapLevel>(value);
					else if (config.HWMipmap == HWMipmapLevel::Off)
						Console.Warning("[GameDB] Game requires mipmapping but it has been force disabled.");
				}
			}
			break;

			case GSHWFixId::TrilinearFiltering:
			{
				if (value >= 0 && value <= static_cast<int>(TriFiltering::Forced))
				{
					if (config.UserHacks_TriFilter == TriFiltering::Automatic)
						config.UserHacks_TriFilter = static_cast<TriFiltering>(value);
					else if (config.UserHacks_TriFilter == TriFiltering::Off)
						Console.Warning("[GameDB] Game requires trilinear filtering but it has been force disabled.");
				}
			}
			break;

			case GSHWFixId::SkipDrawStart:
				config.SkipDrawStart = value;
				break;

			case GSHWFixId::SkipDrawEnd:
				config.SkipDrawEnd = value;
				break;

			case GSHWFixId::HalfBottomOverride:
				config.UserHacks_HalfBottomOverride = value;
				break;

			case GSHWFixId::HalfPixelOffset:
				config.UserHacks_HalfPixelOffset = value;
				break;

			case GSHWFixId::RoundSprite:
				config.UserHacks_RoundSprite = value;
				break;

			case GSHWFixId::TexturePreloading:
			{
				if (value >= 0 && value <= static_cast<int>(TexturePreloadingLevel::Full))
					config.TexturePreloading = std::min(config.TexturePreloading, static_cast<TexturePreloadingLevel>(value));
			}
			break;

			case GSHWFixId::Deinterlace:
				if (value >= 0 && value <= static_cast<int>(GSInterlaceMode::Automatic))
				{
					if (config.InterlaceMode == GSInterlaceMode::Automatic)
						config.InterlaceMode = static_cast<GSInterlaceMode>(value);
					else
						Console.Warning("[GameDB] Game requires different deinterlace mode but it has been overridden by user setting.");
				}
			break;

			default:
				break;
		}

		PatchesCon->WriteLn("[GameDB] Enabled GS Hardware Fix: %s to [mode=%d]", getHWFixName(id), value);
		num_applied_fixes++;
	}

	// fixup skipdraw range just in case the db has a bad range (but the linter should catch this)
	config.SkipDrawEnd = std::max(config.SkipDrawStart, config.SkipDrawEnd);

	return num_applied_fixes;
}

void GameDatabase::initDatabase()
{
	ryml::Callbacks rymlCallbacks = ryml::get_callbacks();
	rymlCallbacks.m_error = [](const char* msg, size_t msg_len, ryml::Location loc, void*) {
		throw std::runtime_error(fmt::format("[YAML] Parsing error at {}:{} (bufpos={}): {}",
			loc.line, loc.col, loc.offset, msg));
	};
	ryml::set_callbacks(rymlCallbacks);
	c4::set_error_callback([](const char* msg, size_t msg_size) {
		throw std::runtime_error(fmt::format("[YAML] Internal Parsing error: {}",
			msg));
	});
	try
	{
		auto buf = Host::ReadResourceFileToString(GAMEDB_YAML_FILE_NAME);
		if (!buf.has_value())
		{
			Console.Error("[GameDB] Unable to open GameDB file, file does not exist.");
			return;
		}

		ryml::Tree tree = ryml::parse_in_arena(c4::to_csubstr(buf.value()));
		ryml::NodeRef root = tree.rootref();

		for (const ryml::NodeRef& n : root.children())
		{
			auto serial = StringUtil::toLower(std::string(n.key().str, n.key().len));

			// Serials and CRCs must be inserted as lower-case, as that is how they are retrieved
			// this is because the application may pass a lowercase CRC or serial along
			//
			// However, YAML's keys are as expected case-sensitive, so we have to explicitly do our own duplicate checking
			if (s_game_db.count(serial) == 1)
			{
				Console.Error(fmt::format("[GameDB] Duplicate serial '{}' found in GameDB. Skipping, Serials are case-insensitive!", serial));
				continue;
			}
			if (n.is_map())
			{
				parseAndInsert(serial, n);
			}
		}
	}
	catch (const std::exception& e)
	{
		Console.Error(fmt::format("[GameDB] Error occured when initializing GameDB: {}", e.what()));
	}
	ryml::reset_callbacks();
}

void GameDatabase::ensureLoaded()
{
	std::call_once(s_load_once_flag, []() {
		Common::Timer timer;
		Console.WriteLn(fmt::format("[GameDB] Has not been initialized yet, initializing..."));
		initDatabase();
		Console.WriteLn("[GameDB] %zu games on record (loaded in %.2fms)", s_game_db.size(), timer.GetTimeMilliseconds());
	});
}

const GameDatabaseSchema::GameEntry* GameDatabase::findGame(const std::string_view& serial)
{
	GameDatabase::ensureLoaded();

	std::string serialLower = StringUtil::toLower(serial);
	Console.WriteLn(fmt::format("[GameDB] Searching for '{}' in GameDB", serialLower));
	const auto gameEntry = s_game_db.find(serialLower);
	if (gameEntry != s_game_db.end())
	{
		Console.WriteLn(fmt::format("[GameDB] Found '{}' in GameDB", serialLower));
		return &gameEntry->second;
	}

	Console.Error(fmt::format("[GameDB] Could not find '{}' in GameDB", serialLower));
	return nullptr;
}
