// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GameDatabase.h"
#include "GS/GS.h"
#include "Host.h"
#include "IconsFontAwesome.h"
#include "vtlb.h"

#include "common/Console.h"
#include "common/EnumOps.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/Timer.h"
#include "common/YAML.h"

#include <sstream>
#include "fmt/format.h"
#include "fmt/ranges.h"
#include <fstream>
#include <mutex>
#include <optional>

namespace GameDatabaseSchema
{
	static const char* getHWFixName(GSHWFixId id);
	static std::optional<GSHWFixId> parseHWFixName(const std::string_view name);
	static bool isUserHackHWFix(GSHWFixId id);
} // namespace GameDatabaseSchema

namespace GameDatabase
{
	static void parseAndInsert(std::string_view serial, const ryml::ConstNodeRef& game_node);
	static std::vector<Patch::DynamicPatchEntry> parseDynamicPatchEntries(std::string_view serial, const ryml::ConstNodeRef& entries_node);
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
	if (crc == 0)
		return nullptr;

	auto it = patches.find(crc);
	if (it != patches.end())
		return &it->second;

	it = patches.find(0);
	if (it != patches.end())
		return &it->second;

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

void GameDatabase::parseAndInsert(const std::string_view serial, const ryml::ConstNodeRef& game_node)
{
	GameDatabaseSchema::GameEntry gameEntry;

	if (const ryml::ConstNodeRef node = game_node.find_child("name"); node.readable() && node.has_val())
		ryml::from_chars(node.val(), &gameEntry.name);

	if (const ryml::ConstNodeRef node = game_node.find_child("name-sort"); node.readable() && node.has_val())
		ryml::from_chars(node.val(), &gameEntry.name_sort);

	if (const ryml::ConstNodeRef node = game_node.find_child("name-en"); node.readable() && node.has_val())
		ryml::from_chars(node.val(), &gameEntry.name_en);

	if (const ryml::ConstNodeRef node = game_node.find_child("region"); node.readable() && node.has_val())
		ryml::from_chars(node.val(), &gameEntry.region);

	if (const ryml::ConstNodeRef node = game_node.find_child("compat"); node.readable() && node.has_val() && node.val().is_integer())
	{
		int value = 0;
		ryml::from_chars(node.val(), &value);
		gameEntry.compat = static_cast<GameDatabaseSchema::Compatibility>(value);
	}

	if (const ryml::ConstNodeRef round_modes = game_node.find_child("roundModes"); round_modes.readable() && round_modes.is_map())
	{
		if (const ryml::ConstNodeRef node = round_modes.find_child("eeRoundMode"); node.readable() && node.has_val() && node.val().is_integer())
		{
			int value = -1;
			ryml::from_chars(node.val(), &value);
			if (value >= 0 && value < static_cast<int>(FPRoundMode::MaxCount))
				gameEntry.eeRoundMode = static_cast<FPRoundMode>(value);
			else
				Console.ErrorFmt("GameDB: Invalid EE round mode '{}', specified for serial: '{}'.", value, serial);
		}
		if (const ryml::ConstNodeRef node = round_modes.find_child("eeDivRoundMode"); node.readable() && node.has_val() && node.val().is_integer())
		{
			int value = -1;
			ryml::from_chars(node.val(), &value);
			if (value >= 0 && value < static_cast<int>(FPRoundMode::MaxCount))
				gameEntry.eeDivRoundMode = static_cast<FPRoundMode>(value);
			else
				Console.ErrorFmt("GameDB: Invalid EE division round mode '{}', specified for serial: '{}'.", value, serial);
		}
		if (const ryml::ConstNodeRef node = round_modes.find_child("vuRoundMode"); node.readable() && node.has_val() && node.val().is_integer())
		{
			int value = -1;
			ryml::from_chars(node.val(), &value);
			if (value >= 0 && value < static_cast<int>(FPRoundMode::MaxCount))
			{
				gameEntry.vu0RoundMode = static_cast<FPRoundMode>(value);
				gameEntry.vu1RoundMode = static_cast<FPRoundMode>(value);
			}
			else
			{
				Console.ErrorFmt("GameDB: Invalid VU round mode '{}', specified for serial: '{}'.", value, serial);
			}
		}
		if (const ryml::ConstNodeRef node = round_modes.find_child("vu0RoundMode"); node.readable() && node.has_val() && node.val().is_integer())
		{
			int value = -1;
			ryml::from_chars(node.val(), &value);
			if (value >= 0 && value < static_cast<int>(FPRoundMode::MaxCount))
				gameEntry.vu0RoundMode = static_cast<FPRoundMode>(value);
			else
				Console.ErrorFmt("GameDB: Invalid VU0 round mode '{}', specified for serial: '{}'.", value, serial);
		}
		if (const ryml::ConstNodeRef node = round_modes.find_child("vu1RoundMode"); node.readable() && node.has_val() && node.val().is_integer())
		{
			int value = -1;
			ryml::from_chars(node.val(), &value);
			if (value >= 0 && value < static_cast<int>(FPRoundMode::MaxCount))
				gameEntry.vu1RoundMode = static_cast<FPRoundMode>(value);
			else
				Console.ErrorFmt("GameDB: Invalid VU1 round mode '{}', specified for serial: '{}'.", value, serial);
		}
	}

	if (const ryml::ConstNodeRef clamp_modes = game_node.find_child("clampModes"); clamp_modes.readable() && clamp_modes.is_map())
	{
		if (const ryml::ConstNodeRef node = clamp_modes.find_child("eeClampMode"); node.readable() && node.has_val() && node.val().is_integer())
		{
			int value = -1;
			ryml::from_chars(node.val(), &value);
			gameEntry.eeClampMode = static_cast<GameDatabaseSchema::ClampMode>(value);
		}
		if (const ryml::ConstNodeRef node = clamp_modes.find_child("vuClampMode"); node.readable() && node.has_val() && node.val().is_integer())
		{
			int value = -1;
			ryml::from_chars(node.val(), &value);
			gameEntry.vu0ClampMode = static_cast<GameDatabaseSchema::ClampMode>(value);
			gameEntry.vu1ClampMode = static_cast<GameDatabaseSchema::ClampMode>(value);
		}
		if (const ryml::ConstNodeRef node = clamp_modes.find_child("vu0ClampMode"); node.readable() && node.has_val() && node.val().is_integer())
		{
			int value = -1;
			ryml::from_chars(node.val(), &value);
			gameEntry.vu0ClampMode = static_cast<GameDatabaseSchema::ClampMode>(value);
		}
		if (const ryml::ConstNodeRef node = clamp_modes.find_child("vu1ClampMode"); node.readable() && node.has_val() && node.val().is_integer())
		{
			int value = -1;
			ryml::from_chars(node.val(), &value);
			gameEntry.vu1ClampMode = static_cast<GameDatabaseSchema::ClampMode>(value);
		}
	}

	// Validate game fixes, invalid ones will be dropped!
	if (const ryml::ConstNodeRef game_fixes = game_node.find_child("gameFixes"); game_fixes.readable() && game_fixes.has_children())
	{
		for (const ryml::ConstNodeRef& game_fix_node : game_fixes.children())
		{
			if (!game_fix_node.has_val())
				continue;

			bool fix_validated = false;
			std::string fix(game_fix_node.val().str, game_fix_node.val().len);

			// Enum values don't end with Hack, but gamedb does, so remove it before comparing.
			if (fix.ends_with("Hack"))
			{
				fix.erase(fix.size() - 4);
				for (GamefixId id = GamefixId_FIRST; id < GamefixId_COUNT; id = static_cast<GamefixId>(enum_cast(id) + 1))
				{
					if (fix.compare(Pcsx2Config::GamefixOptions::GetGameFixName(id)) == 0 &&
						std::find(gameEntry.gameFixes.begin(), gameEntry.gameFixes.end(), id) == gameEntry.gameFixes.end())
					{
						gameEntry.gameFixes.push_back(id);
						fix_validated = true;
						break;
					}
				}
			}

			if (!fix_validated)
			{
				Console.ErrorFmt("GameDB: Invalid gamefix: '{}', specified for serial: '{}'. Dropping!", fix, serial);
			}
		}
	}

	if (const ryml::ConstNodeRef speed_hacks = game_node.find_child("speedHacks"); speed_hacks.readable() && speed_hacks.has_children())
	{
		for (const ryml::ConstNodeRef& node : speed_hacks.children())
		{
			if (!node.has_val())
				continue;

			const std::string_view id_view = std::string_view(node.key().str, node.key().len);
			const std::string_view value_view = std::string_view(node.val().str, node.val().len);
			const std::optional<SpeedHack> id = Pcsx2Config::SpeedhackOptions::ParseSpeedHackName(id_view);
			const std::optional<int> value = StringUtil::FromChars<int>(value_view);

			if (id.has_value() && value.has_value() &&
				std::none_of(gameEntry.speedHacks.begin(), gameEntry.speedHacks.end(),
					[&id](const auto& it) { return it.first == id.value(); }))
			{
				gameEntry.speedHacks.emplace_back(id.value(), value.value());
			}
			else
			{
				Console.ErrorFmt("GameDB: Invalid speedhack: '{}={}', specified for serial: '{}'. Dropping!",
					id_view, value_view, serial);
			}
		}
	}

	if (const ryml::ConstNodeRef gs_hw_fixes = game_node.find_child("gsHWFixes"); gs_hw_fixes.readable() && gs_hw_fixes.has_children())
	{
		for (const ryml::ConstNodeRef& node : gs_hw_fixes.children())
		{
			if (!node.has_val())
				continue;

			const std::string_view id_name(node.key().data(), node.key().size());
			const std::optional<GameDatabaseSchema::GSHWFixId> id = GameDatabaseSchema::parseHWFixName(id_name);
			std::optional<s32> value;
			if (id.has_value() && (id.value() == GameDatabaseSchema::GSHWFixId::GetSkipCount ||
									  id.value() == GameDatabaseSchema::GSHWFixId::BeforeDraw ||
									  id.value() == GameDatabaseSchema::GSHWFixId::MoveHandler))
			{
				const std::string_view str_value(node.has_val() ? std::string_view(node.val().data(), node.val().size()) : std::string_view());
				if (id.value() == GameDatabaseSchema::GSHWFixId::GetSkipCount)
					value = GSLookupGetSkipCountFunctionId(str_value);
				else if (id.value() == GameDatabaseSchema::GSHWFixId::BeforeDraw)
					value = GSLookupBeforeDrawFunctionId(str_value);
				else if (id.value() == GameDatabaseSchema::GSHWFixId::MoveHandler)
					value = GSLookupMoveHandlerFunctionId(str_value);

				if (value.value_or(-1) < 0)
				{
					Console.ErrorFmt("GameDB: Invalid GS HW Fix Value for '{}' in '{}': '{}'", id_name, serial, str_value);
					continue;
				}
			}
			else
			{
				value = node.has_val() ? StringUtil::FromChars<s32>(std::string_view(node.val().data(), node.val().size())) : 1;
			}
			if (!id.has_value() || !value.has_value())
			{
				Console.ErrorFmt("GameDB: Invalid GS HW Fix: '{}' specified for serial '{}'. Dropping!", id_name, serial);
				continue;
			}

			gameEntry.gsHWFixes.emplace_back(id.value(), value.value());
		}
	}

	// Memory Card Filters - Store as a vector to allow flexibility in the future
	// - currently they are used as a '\n' delimited string in the app
	if (const ryml::ConstNodeRef memcard_filters = game_node.find_child("memcardFilters"); memcard_filters.readable() && memcard_filters.has_children())
	{
		for (const ryml::ConstNodeRef& node : memcard_filters.children())
		{
			if (!node.has_val())
				continue;

			std::string memcard_filter(node.val().str, node.val().len);
			gameEntry.memcardFilters.emplace_back(std::move(memcard_filter));
		}
	}

	// Game Patches
	if (const ryml::ConstNodeRef patches = game_node.find_child("patches"); patches.readable() && patches.has_children())
	{
		for (const ryml::ConstNodeRef& patch_node : patches.children())
		{
			// use a crc of 0 for default patches
			const std::string_view crc_str(patch_node.key().str, patch_node.key().len);
			const std::optional<u32> crc = (StringUtil::compareNoCase(crc_str, "default")) ? std::optional<u32>(0) : StringUtil::FromChars<u32>(crc_str, 16);
			if (!crc.has_value())
			{
				Console.ErrorFmt("GameDB: Invalid CRC '{}' found for serial '{}'. Skipping!", crc_str, serial);
				continue;
			}
			if (gameEntry.patches.find(crc.value()) != gameEntry.patches.end())
			{
				Console.ErrorFmt("GameDB: Duplicate CRC '{}' found for serial '{}'. Skipping, CRCs are case-insensitive!", crc_str, serial);
				continue;
			}

			std::string patch;
			if (ryml::ConstNodeRef content_node = patch_node.find_child("content"); content_node.readable())
				ryml::from_chars(content_node.val(), &patch);
			gameEntry.patches.emplace(crc.value(), std::move(patch));
		}
	}

	if (const ryml::ConstNodeRef dyna_patches = game_node.find_child("dynaPatches"); dyna_patches.readable() && dyna_patches.has_children())
	{
		for (const ryml::ConstNodeRef& dyna_patch_node : dyna_patches.children())
		{
			Patch::DynamicPatch& patch = gameEntry.dynaPatches.emplace_back();
			patch.pattern = parseDynamicPatchEntries(serial, dyna_patch_node.at("pattern"));
			patch.replacement = parseDynamicPatchEntries(serial, dyna_patch_node.at("replacement"));
		}
	}

	s_game_db.emplace(std::move(serial), std::move(gameEntry));
}

std::vector<Patch::DynamicPatchEntry> GameDatabase::parseDynamicPatchEntries(const std::string_view serial, const ryml::ConstNodeRef& entries_node)
{
	std::vector<Patch::DynamicPatchEntry> entries;

	if (entries_node.readable() && entries_node.has_children())
	{
		for (const auto& entry_node : entries_node.children())
		{
			ryml::ConstNodeRef offset_node = entry_node.find_child("offset");
			if (!offset_node.readable() || !offset_node.has_val() || !offset_node.val().is_integer())
			{
				Console.ErrorFmt("GameDB: Invalid dynamic patch entry found for serial '{}'.", serial);
				continue;
			}

			ryml::ConstNodeRef value_node = entry_node.find_child("value");
			if (!value_node.readable() || !value_node.has_val() || !value_node.val().is_integer())
			{
				Console.ErrorFmt("GameDB: Invalid dynamic patch entry found for serial '{}'.", serial);
				continue;
			}

			Patch::DynamicPatchEntry& entry = entries.emplace_back();
			ryml::from_chars(offset_node.val(), &entry.offset);
			ryml::from_chars(value_node.val(), &entry.value);
		}
	}

	return entries;
}

static const char* s_round_modes[static_cast<u32>(FPRoundMode::MaxCount)] = {
	"Nearest",
	"NegativeInfinity",
	"PositiveInfinity",
	"Chop"};

static const char* s_gs_hw_fix_names[] = {
	"autoFlush",
	"cpuFramebufferConversion",
	"readTCOnClose",
	"disableDepthSupport",
	"preloadFrameData",
	"disablePartialInvalidation",
	"textureInsideRT",
	"limit24BitDepth",
	"alignSprite",
	"mergeSprite",
	"mipmap",
	"accurateAlphaTest",
	"forceEvenSpritePosition",
	"bilinearUpscale",
	"nativePaletteDraw",
	"estimateTextureRegion",
	"PCRTCOffsets",
	"PCRTCOverscan",
	"trilinearFiltering",
	"skipDrawStart",
	"skipDrawEnd",
	"halfPixelOffset",
	"roundSprite",
	"nativeScaling",
	"texturePreloading",
	"deinterlace",
	"cpuSpriteRenderBW",
	"cpuSpriteRenderLevel",
	"cpuCLUTRender",
	"gpuTargetCLUT",
	"gpuPaletteConversion",
	"minimumBlendingLevel",
	"maximumBlendingLevel",
	"recommendedBlendingLevel",
	"getSkipCount",
	"beforeDraw",
	"moveHandler",
};
static_assert(std::size(s_gs_hw_fix_names) == static_cast<u32>(GameDatabaseSchema::GSHWFixId::Count), "HW fix name lookup is correct size");

const char* GameDatabaseSchema::getHWFixName(GSHWFixId id)
{
	return s_gs_hw_fix_names[static_cast<u32>(id)];
}

static std::optional<GameDatabaseSchema::GSHWFixId> GameDatabaseSchema::parseHWFixName(const std::string_view name)
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
		case GSHWFixId::TrilinearFiltering:
		case GSHWFixId::MinimumBlendingLevel:
		case GSHWFixId::MaximumBlendingLevel:
		case GSHWFixId::RecommendedBlendingLevel:
		case GSHWFixId::PCRTCOffsets:
		case GSHWFixId::PCRTCOverscan:
		case GSHWFixId::GetSkipCount:
		case GSHWFixId::BeforeDraw:
		case GSHWFixId::MoveHandler:
			return false;
		default:
			return true;
	}
}

void GameDatabaseSchema::GameEntry::applyGameFixes(Pcsx2Config& config, bool applyAuto) const
{
	// Only apply core game fixes if the user has enabled them.
	if (!applyAuto)
		Console.Warning("GameDB: Game Fixes are disabled");

	if (eeRoundMode < FPRoundMode::MaxCount)
	{
		if (applyAuto)
		{
			Console.WriteLn("GameDB: Changing EE/FPU roundmode to %d [%s]", eeRoundMode, s_round_modes[static_cast<u8>(eeRoundMode)]);
			config.Cpu.FPUFPCR.SetRoundMode(eeRoundMode);
		}
		else
		{
			Console.Warning("GameDB: Skipping changing EE/FPU roundmode to %d [%s]", eeRoundMode, s_round_modes[static_cast<u8>(eeRoundMode)]);
		}
	}

	if (eeDivRoundMode < FPRoundMode::MaxCount)
	{
		if (applyAuto)
		{
			Console.WriteLn("GameDB: Changing EE/FPU divison roundmode to %d [%s]", eeRoundMode, s_round_modes[static_cast<u8>(eeDivRoundMode)]);
			config.Cpu.FPUDivFPCR.SetRoundMode(eeDivRoundMode);
		}
		else
		{
			Console.Warning("GameDB: Skipping changing EE/FPU roundmode to %d [%s]", eeRoundMode, s_round_modes[static_cast<u8>(eeRoundMode)]);
		}
	}

	if (vu0RoundMode < FPRoundMode::MaxCount)
	{
		if (applyAuto)
		{
			Console.WriteLn("GameDB: Changing VU0 roundmode to %d [%s]", vu0RoundMode, s_round_modes[static_cast<u8>(vu0RoundMode)]);
			config.Cpu.VU0FPCR.SetRoundMode(vu0RoundMode);
		}
		else
		{
			Console.Warning("GameDB: Skipping changing VU0 roundmode to %d [%s]", vu0RoundMode, s_round_modes[static_cast<u8>(vu0RoundMode)]);
		}
	}

	if (vu1RoundMode < FPRoundMode::MaxCount)
	{
		if (applyAuto)
		{
			Console.WriteLn("GameDB: Changing VU1 roundmode to %d [%s]", vu1RoundMode, s_round_modes[static_cast<u8>(vu1RoundMode)]);
			config.Cpu.VU1FPCR.SetRoundMode(vu1RoundMode);
		}
		else
		{
			Console.Warning("GameDB: Skipping changing VU1 roundmode to %d [%s]", vu1RoundMode, s_round_modes[static_cast<u8>(vu1RoundMode)]);
		}
	}

	if (eeClampMode != GameDatabaseSchema::ClampMode::Undefined)
	{
		const int clampMode = enum_cast(eeClampMode);
		if (applyAuto)
		{
			Console.WriteLn("GameDB: Changing EE/FPU clamp mode [mode=%d]", clampMode);
			config.Cpu.Recompiler.fpuOverflow = (clampMode >= 1);
			config.Cpu.Recompiler.fpuExtraOverflow = (clampMode >= 2);
			config.Cpu.Recompiler.fpuFullMode = (clampMode >= 3);
		}
		else
			Console.Warning("GameDB: Skipping changing EE/FPU clamp mode [mode=%d]", clampMode);
	}

	if (vu0ClampMode != GameDatabaseSchema::ClampMode::Undefined)
	{
		const int clampMode = enum_cast(vu0ClampMode);
		if (applyAuto)
		{
			Console.WriteLn("GameDB: Changing VU0 clamp mode [mode=%d]", clampMode);
			config.Cpu.Recompiler.vu0Overflow = (clampMode >= 1);
			config.Cpu.Recompiler.vu0ExtraOverflow = (clampMode >= 2);
			config.Cpu.Recompiler.vu0SignOverflow = (clampMode >= 3);
		}
		else
			Console.Warning("GameDB: Skipping changing VU0 clamp mode [mode=%d]", clampMode);
	}

	if (vu1ClampMode != GameDatabaseSchema::ClampMode::Undefined)
	{
		const int clampMode = enum_cast(vu1ClampMode);
		if (applyAuto)
		{
			Console.WriteLn("GameDB: Changing VU1 clamp mode [mode=%d]", clampMode);
			config.Cpu.Recompiler.vu1Overflow = (clampMode >= 1);
			config.Cpu.Recompiler.vu1ExtraOverflow = (clampMode >= 2);
			config.Cpu.Recompiler.vu1SignOverflow = (clampMode >= 3);
		}
		else
			Console.Warning("GameDB: Skipping changing VU1 clamp mode [mode=%d]", clampMode);
	}

	// TODO - config - this could be simplified with maps instead of bitfields and enums
	for (const auto& it : speedHacks)
	{
		if (!applyAuto)
		{
			Console.Warning("GameDB: Skipping setting Speedhack '%s' to [mode=%d]",
				Pcsx2Config::SpeedhackOptions::GetSpeedHackName(it.first), it.second);
			continue;
		}
		// Legacy note - speedhacks are setup in the GameDB as integer values, but
		// are effectively booleans like the gamefixes
		config.Speedhacks.Set(it.first, it.second);
		Console.WriteLn("GameDB: Setting Speedhack '%s' to [mode=%d]",
			Pcsx2Config::SpeedhackOptions::GetSpeedHackName(it.first), it.second);
	}

	// TODO - config - this could be simplified with maps instead of bitfields and enums
	for (const GamefixId id : gameFixes)
	{
		if (!applyAuto)
		{
			Console.Warning("GameDB: Skipping Gamefix: %s", Pcsx2Config::GamefixOptions::GetGameFixName(id));
			continue;
		}
		// if the fix is present, it is said to be enabled
		config.Gamefixes.Set(id, true);
		Console.WriteLn("GameDB: Enabled Gamefix: %s", Pcsx2Config::GamefixOptions::GetGameFixName(id));

		// The LUT is only used for 1 game so we allocate it only when the gamefix is enabled (save 4MB)
		if (id == Fix_GoemonTlbMiss && true)
			vtlb_Alloc_Ppmap();
	}
}

bool GameDatabaseSchema::GameEntry::configMatchesHWFix(const Pcsx2Config::GSOptions& config, GSHWFixId id, int value)
{
	switch (id)
	{
		case GSHWFixId::AutoFlush:
			return (static_cast<int>(config.UserHacks_AutoFlush) == value);

		case GSHWFixId::CPUFramebufferConversion:
			return (static_cast<int>(config.UserHacks_CPUFBConversion) == value);

		case GSHWFixId::FlushTCOnClose:
			return (static_cast<int>(config.UserHacks_ReadTCOnClose) == value);

		case GSHWFixId::DisableDepthSupport:
			return (static_cast<int>(config.UserHacks_DisableDepthSupport) == value);

		case GSHWFixId::PreloadFrameData:
			return (static_cast<int>(config.PreloadFrameWithGSData) == value);

		case GSHWFixId::DisablePartialInvalidation:
			return (static_cast<int>(config.UserHacks_DisablePartialInvalidation) == value);

		case GSHWFixId::TextureInsideRT:
			return (static_cast<int>(config.UserHacks_TextureInsideRt) == value);

		case GSHWFixId::Limit24BitDepth:
			return (static_cast<int>(config.UserHacks_Limit24BitDepth) == value);

		case GSHWFixId::AlignSprite:
			return (config.UpscaleMultiplier <= 1.0f || static_cast<int>(config.UserHacks_AlignSpriteX) == value);

		case GSHWFixId::MergeSprite:
			return (config.UpscaleMultiplier <= 1.0f || static_cast<int>(config.UserHacks_MergePPSprite) == value);

		case GSHWFixId::ForceEvenSpritePosition:
			return (config.UpscaleMultiplier <= 1.0f || static_cast<int>(config.UserHacks_ForceEvenSpritePosition) == value);

		case GSHWFixId::BilinearUpscale:
			return (config.UpscaleMultiplier <= 1.0f || static_cast<int>(config.UserHacks_BilinearHack) == value);

		case GSHWFixId::NativePaletteDraw:
			return (config.UpscaleMultiplier <= 1.0f || static_cast<int>(config.UserHacks_NativePaletteDraw) == value);

		case GSHWFixId::EstimateTextureRegion:
			return (static_cast<int>(config.UserHacks_EstimateTextureRegion) == value);

		case GSHWFixId::PCRTCOffsets:
			return (static_cast<int>(config.PCRTCOffsets) == value);

		case GSHWFixId::PCRTCOverscan:
			return (static_cast<int>(config.PCRTCOverscan) == value);

		case GSHWFixId::Mipmap:
			return (static_cast<int>(config.HWMipmap) == value);

		case GSHWFixId::AccurateAlphaTest:
			return (static_cast<int>(config.HWAccurateAlphaTest) == value);

		case GSHWFixId::TrilinearFiltering:
			return (config.TriFilter == TriFiltering::Automatic || static_cast<int>(config.TriFilter) == value);

		case GSHWFixId::SkipDrawStart:
			return (config.SkipDrawStart == value);

		case GSHWFixId::SkipDrawEnd:
			return (config.SkipDrawEnd == value);

		case GSHWFixId::HalfPixelOffset:
			return (config.UpscaleMultiplier <= 1.0f || config.UserHacks_HalfPixelOffset == static_cast<GSHalfPixelOffset>(value));

		case GSHWFixId::RoundSprite:
			return (config.UpscaleMultiplier <= 1.0f || config.UserHacks_RoundSprite == value);

		case GSHWFixId::NativeScaling:
			return (config.UpscaleMultiplier <= 1.0f || static_cast<int>(config.UserHacks_NativeScaling) == value);

		case GSHWFixId::TexturePreloading:
			return (static_cast<int>(config.TexturePreloading) <= value);

		case GSHWFixId::Deinterlace:
			return (config.InterlaceMode == GSInterlaceMode::Automatic || static_cast<int>(config.InterlaceMode) == value);

		case GSHWFixId::CPUSpriteRenderBW:
			return (config.UserHacks_CPUSpriteRenderBW == value);

		case GSHWFixId::CPUSpriteRenderLevel:
			return (config.UserHacks_CPUSpriteRenderLevel == value);

		case GSHWFixId::CPUCLUTRender:
			return (config.UserHacks_CPUCLUTRender == value);

		case GSHWFixId::GPUTargetCLUT:
			return (static_cast<int>(config.UserHacks_GPUTargetCLUTMode) == value);

		case GSHWFixId::GPUPaletteConversion:
			return (config.GPUPaletteConversion == ((value > 1) ? (config.TexturePreloading == TexturePreloadingLevel::Full) : (value != 0)));

		case GSHWFixId::MinimumBlendingLevel:
			return (static_cast<int>(config.AccurateBlendingUnit) >= value);

		case GSHWFixId::MaximumBlendingLevel:
			return (static_cast<int>(config.AccurateBlendingUnit) <= value);

		case GSHWFixId::RecommendedBlendingLevel:
			return true;

		case GSHWFixId::GetSkipCount:
			return (static_cast<int>(config.GetSkipCountFunctionId) == value);

		case GSHWFixId::BeforeDraw:
			return (static_cast<int>(config.BeforeDrawFunctionId) == value);

		case GSHWFixId::MoveHandler:
			return (static_cast<int>(config.MoveHandlerFunctionId) == value);

		default:
			return false;
	}
}

void GameDatabaseSchema::GameEntry::applyGSHardwareFixes(Pcsx2Config::GSOptions& config) const
{
	std::string disabled_fixes;

	// Only apply GS HW fixes if the user hasn't manually enabled HW fixes.
	const bool apply_auto_fixes = !config.ManualUserHacks;
	const bool is_sw_renderer = EmuConfig.GS.Renderer == GSRendererType::SW;
	if (!apply_auto_fixes)
		Console.Warning("GameDB: Manual GS hardware renderer fixes are enabled, not using automatic hardware renderer fixes from GameDB.");

	for (const auto& [id, value] : gsHWFixes)
	{
		if (isUserHackHWFix(id) && !apply_auto_fixes)
		{
			if (configMatchesHWFix(config, id, value))
				continue;

			Console.Warning("GameDB: Skipping GS Hardware Fix: %s to [mode=%d]", getHWFixName(id), value);
			fmt::format_to(std::back_inserter(disabled_fixes), "{} {} = {}", disabled_fixes.empty() ? "  " : "\n  ", getHWFixName(id), value);
			continue;
		}

		switch (id)
		{
			case GSHWFixId::AutoFlush:
			{
				if (value >= 0 && value <= static_cast<int>(GSHWAutoFlushLevel::Enabled))
					config.UserHacks_AutoFlush = static_cast<GSHWAutoFlushLevel>(value);
			}
			break;

			case GSHWFixId::CPUFramebufferConversion:
				config.UserHacks_CPUFBConversion = (value > 0);
				break;

			case GSHWFixId::FlushTCOnClose:
				config.UserHacks_ReadTCOnClose = (value > 0);
				break;

			case GSHWFixId::DisableDepthSupport:
				config.UserHacks_DisableDepthSupport = (value > 0);
				break;

			case GSHWFixId::PreloadFrameData:
				config.PreloadFrameWithGSData = (value > 0);
				break;

			case GSHWFixId::DisablePartialInvalidation:
				config.UserHacks_DisablePartialInvalidation = (value > 0);
				break;

			case GSHWFixId::TextureInsideRT:
			{
				if (value >= 0 && value <= static_cast<int>(GSTextureInRtMode::MergeTargets))
					config.UserHacks_TextureInsideRt = static_cast<GSTextureInRtMode>(value);
			}
			break;

			case GSHWFixId::Limit24BitDepth:
			{
				if (value >= 0 && value <= static_cast<int>(GSLimit24BitDepth::PrioritizeLower))
					config.UserHacks_Limit24BitDepth = static_cast<GSLimit24BitDepth>(value);
			}
			break;

			case GSHWFixId::AlignSprite:
				config.UserHacks_AlignSpriteX = (value > 0);
				break;

			case GSHWFixId::MergeSprite:
				config.UserHacks_MergePPSprite = (value > 0);
				break;

			case GSHWFixId::ForceEvenSpritePosition:
				config.UserHacks_ForceEvenSpritePosition = (value > 0);
				break;

			case GSHWFixId::BilinearUpscale:
			{
				if (value >= 0 && value < static_cast<int>(GSBilinearDirtyMode::MaxCount))
					config.UserHacks_BilinearHack = static_cast<GSBilinearDirtyMode>(value);
			}
			break;

			case GSHWFixId::NativePaletteDraw:
				config.UserHacks_NativePaletteDraw = (value > 0);
				break;

			case GSHWFixId::EstimateTextureRegion:
				config.UserHacks_EstimateTextureRegion = (value > 0);
				break;

			case GSHWFixId::PCRTCOffsets:
				config.PCRTCOffsets = (value > 0);
				break;

			case GSHWFixId::PCRTCOverscan:
				config.PCRTCOverscan = (value > 0);
				break;

			case GSHWFixId::Mipmap:
				config.HWMipmap = (value > 0);
				break;

			case GSHWFixId::AccurateAlphaTest:
				config.HWAccurateAlphaTest = (value > 0);
				break;

			case GSHWFixId::TrilinearFiltering:
			{
				if (value >= 0 && value <= static_cast<int>(TriFiltering::Forced))
				{
					if (config.TriFilter == TriFiltering::Automatic)
						config.TriFilter = static_cast<TriFiltering>(value);
					else if (config.TriFilter > TriFiltering::Off)
						Console.Warning("GameDB: Game requires trilinear filtering to be disabled.");
				}
			}
			break;

			case GSHWFixId::SkipDrawStart:
				config.SkipDrawStart = value;
				break;

			case GSHWFixId::SkipDrawEnd:
				config.SkipDrawEnd = value;
				break;

			case GSHWFixId::HalfPixelOffset:
			{
				if (value >= 0 && value < static_cast<int>(GSHalfPixelOffset::MaxCount))
					config.UserHacks_HalfPixelOffset = static_cast<GSHalfPixelOffset>(value);
			}
			break;

			case GSHWFixId::RoundSprite:
				config.UserHacks_RoundSprite = value;
				break;

			case GSHWFixId::NativeScaling:
				config.UserHacks_NativeScaling = static_cast<GSNativeScaling>(value);
				break;

			case GSHWFixId::TexturePreloading:
			{
				if (value >= 0 && value <= static_cast<int>(TexturePreloadingLevel::Full))
					config.TexturePreloading = std::min(config.TexturePreloading, static_cast<TexturePreloadingLevel>(value));
			}
			break;

			case GSHWFixId::Deinterlace:
			{
				if (value >= static_cast<int>(GSInterlaceMode::Automatic) && value < static_cast<int>(GSInterlaceMode::Count))
				{
					if (config.InterlaceMode == GSInterlaceMode::Automatic)
						config.InterlaceMode = static_cast<GSInterlaceMode>(value);
					else
						Console.Warning("GameDB: Game requires different deinterlace mode but it has been overridden by user setting.");
				}
			}
			break;

			case GSHWFixId::CPUSpriteRenderBW:
				config.UserHacks_CPUSpriteRenderBW = value;
				break;

			case GSHWFixId::CPUSpriteRenderLevel:
				config.UserHacks_CPUSpriteRenderLevel = value;
				break;

			case GSHWFixId::CPUCLUTRender:
				config.UserHacks_CPUCLUTRender = value;
				break;

			case GSHWFixId::GPUTargetCLUT:
			{
				if (value >= 0 && value <= static_cast<int>(GSGPUTargetCLUTMode::InsideTarget))
					config.UserHacks_GPUTargetCLUTMode = static_cast<GSGPUTargetCLUTMode>(value);
			}
			break;

			case GSHWFixId::GPUPaletteConversion:
			{
				// if 2, enable paltex when preloading is full, otherwise leave as-is
				if (value > 1)
					config.GPUPaletteConversion = (config.TexturePreloading == TexturePreloadingLevel::Full) ? true : config.GPUPaletteConversion;
				else
					config.GPUPaletteConversion = (value != 0);
			}
			break;

			case GSHWFixId::MinimumBlendingLevel:
			{
				if (value >= 0 && value <= static_cast<int>(AccBlendLevel::Maximum))
					config.AccurateBlendingUnit = std::max(config.AccurateBlendingUnit, static_cast<AccBlendLevel>(value));
			}
			break;

			case GSHWFixId::MaximumBlendingLevel:
			{
				if (value >= 0 && value <= static_cast<int>(AccBlendLevel::Maximum))
					config.AccurateBlendingUnit = std::min(config.AccurateBlendingUnit, static_cast<AccBlendLevel>(value));
			}
			break;

			case GSHWFixId::RecommendedBlendingLevel:
			{
				if (!is_sw_renderer && value >= 0 && value <= static_cast<int>(AccBlendLevel::Maximum) && static_cast<int>(EmuConfig.GS.AccurateBlendingUnit) < value)
				{
					static constexpr std::array<const char*, static_cast<u8>(AccBlendLevel::MaxCount)> s_blending_option_names = {{
						TRANSLATE_NOOP("GameDatabase", "Minimum"),
						TRANSLATE_NOOP("GameDatabase", "Basic"),
						TRANSLATE_NOOP("GameDatabase", "Medium"),
						TRANSLATE_NOOP("GameDatabase", "High"),
						TRANSLATE_NOOP("GameDatabase", "Full"),
						TRANSLATE_NOOP("GameDatabase", "Maximum"),
					}};

					Host::AddKeyedOSDMessage("HWBlendingWarning",
						fmt::format(TRANSLATE_FS("GameDatabase",
										"{0} Current Blending Accuracy is {1}.\n"
										"Recommended Blending Accuracy for this game is {2}.\n"
										"You can adjust the blending level in Game Properties to improve\n"
										"graphical quality, but this will increase system requirements."),
							ICON_FA_PAINTBRUSH,
							s_blending_option_names[static_cast<u8>(EmuConfig.GS.AccurateBlendingUnit)],
							s_blending_option_names[static_cast<u8>(value)]),
						Host::OSD_WARNING_DURATION);
				}
				else
				{
					Host::RemoveKeyedOSDMessage("HWBlendingWarning");
				}
			}
			break;

			case GSHWFixId::GetSkipCount:
				config.GetSkipCountFunctionId = static_cast<s16>(value);
				break;

			case GSHWFixId::BeforeDraw:
				config.BeforeDrawFunctionId = static_cast<s16>(value);
				break;

			case GSHWFixId::MoveHandler:
				config.MoveHandlerFunctionId = static_cast<s16>(value);
				break;

			default:
				break;
		}

		Console.WriteLn("GameDB: Enabled GS Hardware Fix: %s to [mode=%d]", getHWFixName(id), value);
	}

	// fixup skipdraw range just in case the db has a bad range (but the linter should catch this)
	config.SkipDrawEnd = std::max(config.SkipDrawStart, config.SkipDrawEnd);

	if (!is_sw_renderer && !disabled_fixes.empty())
	{
		Host::AddKeyedOSDMessage("HWFixesWarning",
			fmt::format(ICON_FA_WAND_MAGIC_SPARKLES " {}\n{}",
				TRANSLATE_SV("GameDatabase", "Manual GS hardware renderer fixes are enabled, automatic fixes were not applied:"),
				disabled_fixes),
			Host::OSD_ERROR_DURATION);
	}
	else
	{
		Host::RemoveKeyedOSDMessage("HWFixesWarning");
	}
}

void GameDatabase::initDatabase()
{
	const std::string path(Path::Combine(EmuFolders::Resources, GAMEDB_YAML_FILE_NAME));
	const std::string name(GAMEDB_YAML_FILE_NAME);

	const std::optional<std::string> buffer = FileSystem::ReadFileToString(path.c_str());
	if (!buffer.has_value())
	{
		Console.Error("GameDB: Unable to open GameDB file, file does not exist.");
		return;
	}

	const ryml::csubstr yaml = ryml::to_csubstr(*buffer);

	Error error;
	const std::optional<ryml::Tree> tree = ParseYAMLFromString(yaml, ryml::to_csubstr(name), &error);
	if (!tree.has_value())
	{
		Console.ErrorFmt("GameDB: Failed to parse game database file {}:", path);
		Console.Error(error.GetDescription());
		return;
	}

	ryml::ConstNodeRef root = tree->crootref();

	for (const ryml::ConstNodeRef& game_node : root.children())
	{
		auto serial = StringUtil::toLower(std::string(game_node.key().str, game_node.key().len));

		// Serials and CRCs must be inserted as lower-case, as that is how they are retrieved
		// this is because the application may pass a lowercase CRC or serial along
		//
		// However, YAML's keys are as expected case-sensitive, so we have to explicitly do our own duplicate checking
		if (s_game_db.count(serial) == 1)
		{
			Console.ErrorFmt("GameDB: Duplicate serial '{}' found in GameDB. Skipping, Serials are case-insensitive!", serial);
			continue;
		}

		if (!game_node.is_map())
			continue;

		parseAndInsert(serial, game_node);
	}
}

void GameDatabase::ensureLoaded()
{
	std::call_once(s_load_once_flag, []() {
		Common::Timer timer;
		Console.WriteLn(fmt::format("GameDB: Has not been initialized yet, initializing..."));
		initDatabase();
		Console.WriteLn("GameDB: %zu games on record (loaded in %.2fms)", s_game_db.size(), timer.GetTimeMilliseconds());
	});
}

const GameDatabaseSchema::GameEntry* GameDatabase::findGame(const std::string_view serial)
{
	GameDatabase::ensureLoaded();

	auto iter = s_game_db.find(StringUtil::toLower(serial));
	return (iter != s_game_db.end()) ? &iter->second : nullptr;
}

bool GameDatabase::TrackHash::parseHash(const std::string_view str)
{
	constexpr u32 expected_length = SIZE * 2;
	if (str.length() != expected_length)
		return false;

	std::memset(data, 0, sizeof(data));
	for (u32 i = 0; i < SIZE * 2; i++)
	{
		const char ch = str[i];
		u8 b;
		if (ch >= '0' && ch <= '9')
			b = static_cast<u8>(ch - '0');
		else if (ch >= 'a' && ch <= 'f')
			b = static_cast<u8>(ch - 'a') + 0xa;
		else if (ch >= 'A' && ch <= 'F')
			b = static_cast<u8>(ch - 'A') + 0xa;
		else
			return false;

		data[i / 2] |= ((i % 2) == 0) ? (b << 4) : b;
	}

	return true;
}

std::string GameDatabase::TrackHash::toString() const
{
	return fmt::format(
		"{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
		data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
		data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
}

struct TrackHashHasher
{
	size_t operator()(const GameDatabase::TrackHash& hash) const
	{
		return std::hash<std::string_view>()(std::string_view(reinterpret_cast<const char*>(hash.data),
			GameDatabase::TrackHash::SIZE));
	}
};

static constexpr char HASHDB_YAML_FILE_NAME[] = "RedumpDatabase.yaml";
std::unordered_map<GameDatabase::TrackHash, u32, TrackHashHasher> s_track_hash_to_entry_map;
std::vector<GameDatabase::HashDatabaseEntry> s_hash_database;

static bool parseHashDatabaseEntry(const ryml::NodeRef& node)
{
	if (!node.has_child("name") || !node.has_child("hashes"))
	{
		Console.Warning("[HashDatabase] Incomplete entry found.");
		return false;
	}

	GameDatabase::HashDatabaseEntry entry;
	node["name"] >> entry.name;
	if (node.has_child("version"))
		node["version"] >> entry.version;
	if (node.has_child("serial"))
		node["serial"] >> entry.serial;

	const u32 index = static_cast<u32>(s_hash_database.size());
	for (const ryml::ConstNodeRef& n : node["hashes"].children())
	{
		if (!n.is_map() || !n.has_child("size") || !n.has_child("md5"))
		{
			Console.ErrorFmt("[HashDatabase] Incomplete hash definition in {}", entry.name);
			return false;
		}

		GameDatabase::TrackHash th;
		std::string md5;
		n["md5"] >> md5;
		n["size"] >> th.size;

		if (!th.parseHash(md5))
		{
			Console.ErrorFmt("[HashDatabase] Failed to parse hash in {}: '{}'", entry.name, md5);
			return false;
		}

		if (entry.tracks.empty() && s_track_hash_to_entry_map.find(th) != s_track_hash_to_entry_map.end())
			Console.WarningFmt("[HashDatabase] Duplicate first track hash in {}", entry.name);

		entry.tracks.push_back(th);
		s_track_hash_to_entry_map.emplace(th, index);
	}

	s_hash_database.push_back(std::move(entry));
	return true;
}

bool GameDatabase::loadHashDatabase()
{
	if (!s_hash_database.empty())
		return true;

	Common::Timer load_timer;

	const std::string path(Path::Combine(EmuFolders::Resources, HASHDB_YAML_FILE_NAME));
	const std::string name(HASHDB_YAML_FILE_NAME);

	std::optional<std::string> buffer = FileSystem::ReadFileToString(path.c_str());
	if (!buffer.has_value())
	{
		Console.Error("[HashDatabase] Unable to open hash database file, file does not exist.");
		return false;
	}

	ryml::csubstr yaml = ryml::to_csubstr(*buffer);

	Error error;
	std::optional<ryml::Tree> tree = ParseYAMLFromString(yaml, ryml::to_csubstr(name), &error);
	if (!tree.has_value())
	{
		Console.ErrorFmt("[HashDatabase] Failed to parse hash database file {}:", path);
		Console.Error(error.GetDescription());
		return false;
	}

	ryml::NodeRef root = tree->rootref();

	bool okay = true;
	for (const ryml::NodeRef& n : root.children())
	{
		if (!parseHashDatabaseEntry(n))
		{
			okay = false;
			break;
		}
	}

	if (!okay)
	{
		s_track_hash_to_entry_map.clear();
		s_hash_database.clear();
		return false;
	}

	Console.WriteLn(Color_StrongGreen, "[HashDatabase] Loaded YAML in %.0f ms", load_timer.GetTimeMilliseconds());
	return true;
}

void GameDatabase::unloadHashDatabase()
{
	s_track_hash_to_entry_map.clear();
	s_hash_database.clear();
}

static size_t getTrackIndex(const GameDatabase::TrackHash* tracks, size_t num_tracks, const GameDatabase::TrackHash& track)
{
	for (size_t i = 0; i < num_tracks; i++)
	{
		if (tracks[i] == track)
			return i;
	}
	return num_tracks;
}

const GameDatabase::HashDatabaseEntry* GameDatabase::lookupHash(
	const TrackHash* tracks, size_t num_tracks, bool* tracks_matched, std::string* match_error)
{
	loadHashDatabase();

	if (num_tracks == 0)
	{
		*match_error = TRANSLATE_STR("GameDatabase", "No tracks provided.");
		std::memset(tracks_matched, 0, sizeof(bool) * num_tracks);
		return nullptr;
	}

	// match the first track, for DVDs this will be all there is anyway
	const auto data_iter = s_track_hash_to_entry_map.find(tracks[0]);
	if (data_iter == s_track_hash_to_entry_map.end())
	{
		*match_error = fmt::format(TRANSLATE_FS("GameDatabase", "Hash {} is not in database."), tracks[0].toString());
		std::memset(tracks_matched, 0, sizeof(bool) * num_tracks);
		return nullptr;
	}

	// make sure they're not missing the data track
	const GameDatabase::HashDatabaseEntry* candidate = &s_hash_database[data_iter->second];
	if (getTrackIndex(candidate->tracks.data(), candidate->tracks.size(), tracks[0]) != 0)
	{
		*match_error = TRANSLATE_STR("GameDatabase", "Data track number does not match data track in database.");
		std::memset(tracks_matched, 0, sizeof(bool) * num_tracks);
		return nullptr;
	}

	// first track is okay!
	tracks_matched[0] = true;
	match_error->clear();

	// now check any audio tracks...
	bool all_okay = true;
	for (size_t track = 1; track < num_tracks; track++)
	{
		const auto audio_iter = s_track_hash_to_entry_map.find(tracks[track]);
		if (audio_iter != s_track_hash_to_entry_map.end())
		{
			fmt::format_to(std::back_inserter(*match_error),
				TRANSLATE_FS("GameDatabase", "Track {0} with hash {1} is not found in database.\n"), track + 1,
				tracks[track].toString());
			tracks_matched[track] = false;
			all_okay = false;
			continue;
		}

		// same game?
		if (audio_iter->second != data_iter->second)
		{
			fmt::format_to(std::back_inserter(*match_error),
				TRANSLATE_FS("GameDatabase", "Track {0} with hash {1} is for a different game ({2}).\n"), track + 1,
				tracks[track].toString(), s_hash_database[audio_iter->second].name);
			tracks_matched[track] = false;
			all_okay = false;
			continue;
		}

		// make sure it's the correct track number
		if (getTrackIndex(candidate->tracks.data(), candidate->tracks.size(), tracks[track]) != track)
		{
			fmt::format_to(std::back_inserter(*match_error),
				TRANSLATE_FS("GameDatabase", "Track {0} with hash {1} does not match database track.\n"), track + 1,
				tracks[track].toString());
			tracks_matched[track] = false;
			all_okay = false;
			continue;
		}

		tracks_matched[track] = true;
	}

	if (!match_error->empty() && match_error->back() == '\n')
		match_error->pop_back();

	return all_okay ? candidate : nullptr;
}
