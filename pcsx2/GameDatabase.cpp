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
#include "Config.h"
#include "Host.h"

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include "fmt/core.h"
#include "fmt/ranges.h"
#include "yaml-cpp/yaml.h"
#include <fstream>
#include <algorithm>
#include <mutex>
#include <thread>

static constexpr char GAMEDB_YAML_FILE_NAME[] = "GameIndex.yaml";
static constexpr char GAMEDB_CACHE_FILE_NAME[] = "gamedb.cache";
static constexpr u64 CACHE_FILE_MAGIC = UINT64_C(0x47414D4544423031); // GAMEDB01

static std::unordered_map<std::string, GameDatabaseSchema::GameEntry> s_game_db;
static std::once_flag s_load_once_flag;

static std::string strToLower(std::string str)
{
	std::transform(str.begin(), str.end(), str.begin(),
		[](unsigned char c) { return std::tolower(c); });
	return str;
}

static bool compareStrNoCase(const std::string& str1, const std::string& str2)
{
	return std::equal(str1.begin(), str1.end(), str2.begin(),
		[](char a, char b) {
			return tolower(a) == tolower(b);
		});
}

std::string GameDatabaseSchema::GameEntry::MemcardFiltersAsString() const
{
	return fmt::to_string(fmt::join(memcardFilters, "/"));
}

const GameDatabaseSchema::Patch* GameDatabaseSchema::GameEntry::FindPatch(const std::string& crc) const
{
	const std::string crcLower(strToLower(crc));
	Console.WriteLn(fmt::format("[GameDB] Searching for patch with CRC '{}'", crc));

	auto it = patches.find(crcLower);
	if (it != patches.end())
	{
		Console.WriteLn(fmt::format("[GameDB] Found patch with CRC '{}'", crc));
		return &it->second;
	}

	it = patches.find("default");
	if (it != patches.end())
	{
		Console.WriteLn("[GameDB] Found and falling back to default patch");
		return &it->second;
	}
	Console.WriteLn("[GameDB] No CRC-specific patch or default patch found");
	return nullptr;
}

const char* GameDatabaseSchema::compatToString(Compatibility compat)
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

static std::vector<std::string> convertMultiLineStringToVector(const std::string& multiLineString)
{
	std::vector<std::string> lines;
	std::istringstream stream(multiLineString);
	std::string line;

	while (std::getline(stream, line))
	{
		lines.push_back(line);
	}

	return lines;
}

static bool parseAndInsert(std::string serial, const YAML::Node& node)
{
	GameDatabaseSchema::GameEntry gameEntry;
	try
	{
		gameEntry.name = node["name"].as<std::string>("");
		gameEntry.region = node["region"].as<std::string>("");
		gameEntry.compat = static_cast<GameDatabaseSchema::Compatibility>(node["compat"].as<int>(enum_cast(gameEntry.compat)));
		// Safely grab round mode and clamp modes from the YAML, otherwise use defaults
		if (YAML::Node roundModeNode = node["roundModes"])
		{
			gameEntry.eeRoundMode = static_cast<GameDatabaseSchema::RoundMode>(roundModeNode["eeRoundMode"].as<int>(enum_cast(gameEntry.eeRoundMode)));
			gameEntry.vuRoundMode = static_cast<GameDatabaseSchema::RoundMode>(roundModeNode["vuRoundMode"].as<int>(enum_cast(gameEntry.vuRoundMode)));
		}
		if (YAML::Node clampModeNode = node["clampModes"])
		{
			gameEntry.eeClampMode = static_cast<GameDatabaseSchema::ClampMode>(clampModeNode["eeClampMode"].as<int>(enum_cast(gameEntry.eeClampMode)));
			gameEntry.vuClampMode = static_cast<GameDatabaseSchema::ClampMode>(clampModeNode["vuClampMode"].as<int>(enum_cast(gameEntry.vuClampMode)));
		}

		// Validate game fixes, invalid ones will be dropped!
		for (std::string& fix : node["gameFixes"].as<std::vector<std::string>>(std::vector<std::string>()))
		{
			bool fixValidated = false;
			for (GamefixId id = GamefixId_FIRST; id < pxEnumEnd; id++)
			{
				std::string validFix = fmt::format("{}Hack", wxString(EnumToString(id)).ToUTF8());
				if (validFix == fix)
				{
					fixValidated = true;
					break;
				}
			}
			if (fixValidated)
			{
				gameEntry.gameFixes.push_back(fix);
			}
			else
			{
				Console.Error(fmt::format("[GameDB] Invalid gamefix: '{}', specified for serial: '{}'. Dropping!", fix, serial));
			}
		}

		// Validate speed hacks, invalid ones will be dropped!
		if (YAML::Node speedHacksNode = node["speedHacks"])
		{
			for (const auto& entry : speedHacksNode)
			{
				std::string speedHack = entry.first.as<std::string>();
				bool speedHackValidated = false;
				for (SpeedhackId id = SpeedhackId_FIRST; id < pxEnumEnd; id++)
				{
					std::string validSpeedHack = fmt::format("{}SpeedHack", wxString(EnumToString(id)).ToUTF8());
					if (validSpeedHack == speedHack)
					{
						speedHackValidated = true;
						break;
					}
				}
				if (speedHackValidated)
				{
					gameEntry.speedHacks[speedHack] = entry.second.as<int>();
				}
				else
				{
					Console.Error(fmt::format("[GameDB] Invalid speedhack: '{}', specified for serial: '{}'. Dropping!", speedHack, serial));
				}
			}
		}

		gameEntry.memcardFilters = node["memcardFilters"].as<std::vector<std::string>>(std::vector<std::string>());

		if (YAML::Node patches = node["patches"])
		{
			for (const auto& entry : patches)
			{
				std::string crc = strToLower(entry.first.as<std::string>());
				if (gameEntry.patches.count(crc) == 1)
				{
					Console.Error(fmt::format("[GameDB] Duplicate CRC '{}' found for serial: '{}'. Skipping, CRCs are case-insensitive!", crc, serial));
					continue;
				}
				YAML::Node patchNode = entry.second;

				gameEntry.patches[crc] = convertMultiLineStringToVector(patchNode["content"].as<std::string>(""));
			}
		}

		s_game_db.emplace(std::move(serial), std::move(gameEntry));
		return true;
	}
	catch (const YAML::RepresentationException& e)
	{
		Console.Error(fmt::format("[GameDB] Invalid GameDB syntax detected on serial: '{}'. Error Details - {}", serial, e.msg));
	}
	catch (const std::exception& e)
	{
		Console.Error(fmt::format("[GameDB] Unexpected error occurred when reading serial: '{}'. Error Details - {}", serial, e.what()));
	}

	return false;
}

static bool LoadYamlFile()
{
	try
	{
		std::optional<std::string> file_data(Host::ReadResourceFileToString(GAMEDB_YAML_FILE_NAME));
		if (!file_data.has_value())
		{
			Console.Error("[GameDB] Unable to open GameDB file.");
			return false;
		}
		// yaml-cpp has memory leak issues if you persist and modify a YAML::Node
		// convert to a map and throw it away instead!
		YAML::Node data = YAML::Load(file_data.value());
		for (const auto& entry : data)
		{
			// we don't want to throw away the entire GameDB file if a single entry is made incorrectly,
			// but we do want to yell about it so it can be corrected
			try
			{
				// Serials and CRCs must be inserted as lower-case, as that is how they are retrieved
				// this is because the application may pass a lowercase CRC or serial along
				//
				// However, YAML's keys are as expected case-sensitive, so we have to explicitly do our own duplicate checking
				std::string serial(strToLower(entry.first.as<std::string>()));
				if (s_game_db.count(serial) == 1)
				{
					Console.Error(fmt::format("[GameDB] Duplicate serial '{}' found in GameDB. Skipping, Serials are case-insensitive!", serial));
					continue;
				}
				parseAndInsert(std::move(serial), entry.second);
			}
			catch (const YAML::RepresentationException& e)
			{
				Console.Error(fmt::format("[GameDB] Invalid GameDB syntax detected. Error Details - {}", e.msg));
			}
		}
	}
	catch (const std::exception& e)
	{
		Console.Error(fmt::format("[GameDB] Error occured when initializing GameDB: {}", e.what()));
		return false;
	}

	return true;
}

static bool ReadString(std::FILE* stream, std::string* dest)
{
	u32 size;
	if (std::fread(&size, sizeof(size), 1, stream) != 1)
		return false;

	dest->resize(size);
	if (size > 0 && std::fread(dest->data(), size, 1, stream) != 1)
		return false;

	return true;
}

static bool ReadS8(std::FILE* stream, s8* dest)
{
	return std::fread(dest, sizeof(s8), 1, stream) > 0;
}

static bool ReadU8(std::FILE* stream, u8* dest)
{
	return std::fread(dest, sizeof(u8), 1, stream) > 0;
}

static bool ReadS32(std::FILE* stream, s32* dest)
{
	return std::fread(dest, sizeof(s32), 1, stream) > 0;
}

static bool ReadU32(std::FILE* stream, u32* dest)
{
	return std::fread(dest, sizeof(u32), 1, stream) > 0;
}

static bool ReadS64(std::FILE* stream, s64* dest)
{
	return std::fread(dest, sizeof(s64), 1, stream) > 0;
}

static bool ReadU64(std::FILE* stream, u64* dest)
{
	return std::fread(dest, sizeof(u64), 1, stream) > 0;
}

static bool WriteString(std::FILE* stream, const std::string& str)
{
	const u32 size = static_cast<u32>(str.size());
	return (std::fwrite(&size, sizeof(size), 1, stream) > 0 &&
			(size == 0 || std::fwrite(str.data(), size, 1, stream) > 0));
}

static bool WriteS8(std::FILE* stream, s8 dest)
{
	return std::fwrite(&dest, sizeof(s8), 1, stream) > 0;
}

static bool WriteU8(std::FILE* stream, u8 dest)
{
	return std::fwrite(&dest, sizeof(u8), 1, stream) > 0;
}

static bool WriteS32(std::FILE* stream, s32 dest)
{
	return std::fwrite(&dest, sizeof(s32), 1, stream) > 0;
}

static bool WriteU32(std::FILE* stream, u32 dest)
{
	return std::fwrite(&dest, sizeof(u32), 1, stream) > 0;
}

static bool WriteS64(std::FILE* stream, s64 dest)
{
	return std::fwrite(&dest, sizeof(s64), 1, stream) > 0;
}

static bool WriteU64(std::FILE* stream, u64 dest)
{
	return std::fwrite(&dest, sizeof(u64), 1, stream) > 0;
}

static s64 GetExpectedMTime()
{
	const std::string yaml_filename(Path::CombineStdString(EmuFolders::Resources, GAMEDB_YAML_FILE_NAME));

	FILESYSTEM_STAT_DATA yaml_sd;
	if (!FileSystem::StatFile(yaml_filename.c_str(), &yaml_sd))
		return -1;

	return yaml_sd.ModificationTime;
}

static bool CheckAndLoad(const char* cached_filename, s64 expected_mtime)
{
	auto fp = FileSystem::OpenManagedCFile(cached_filename, "rb");
	if (!fp)
		return false;

	u64 file_signature;
	s64 file_mtime, start_pos, file_size;
	if (!ReadU64(fp.get(), &file_signature) || file_signature != CACHE_FILE_MAGIC ||
		!ReadS64(fp.get(), &file_mtime) || file_mtime != expected_mtime ||
		(start_pos = FileSystem::FTell64(fp.get())) < 0 || FileSystem::FSeek64(fp.get(), 0, SEEK_END) != 0 ||
		(file_size = FileSystem::FTell64(fp.get())) < 0 || FileSystem::FSeek64(fp.get(), start_pos, SEEK_SET) != 0)
	{
		return false;
	}

	while (FileSystem::FTell64(fp.get()) != file_size)
	{
		std::string serial;
		GameDatabaseSchema::GameEntry entry;
		u8 compat;
		s8 ee_round, ee_clamp, vu_round, vu_clamp;
		u32 game_fix_count, speed_hack_count, memcard_filter_count, patch_count;

		if (!ReadString(fp.get(), &serial) ||
			!ReadString(fp.get(), &entry.name) ||
			!ReadString(fp.get(), &entry.region) ||
			!ReadU8(fp.get(), &compat) || compat > static_cast<u8>(GameDatabaseSchema::Compatibility::Perfect) ||
			!ReadS8(fp.get(), &ee_round) || ee_round < static_cast<s8>(GameDatabaseSchema::RoundMode::Undefined) || ee_round > static_cast<s8>(GameDatabaseSchema::RoundMode::ChopZero) ||
			!ReadS8(fp.get(), &ee_clamp) || ee_clamp < static_cast<s8>(GameDatabaseSchema::ClampMode::Undefined) || ee_clamp > static_cast<s8>(GameDatabaseSchema::ClampMode::Full) ||
			!ReadS8(fp.get(), &vu_round) || vu_round < static_cast<s8>(GameDatabaseSchema::RoundMode::Undefined) || vu_round > static_cast<s8>(GameDatabaseSchema::RoundMode::ChopZero) ||
			!ReadS8(fp.get(), &vu_clamp) || vu_clamp < static_cast<s8>(GameDatabaseSchema::ClampMode::Undefined) || vu_clamp > static_cast<s8>(GameDatabaseSchema::ClampMode::Full) ||
			!ReadU32(fp.get(), &game_fix_count) ||
			!ReadU32(fp.get(), &speed_hack_count) ||
			!ReadU32(fp.get(), &memcard_filter_count) ||
			!ReadU32(fp.get(), &patch_count))
		{
			Console.Error("GameDB: Read error while loading entry");
			return false;
		}

		entry.compat = static_cast<GameDatabaseSchema::Compatibility>(compat);
		entry.eeRoundMode = static_cast<GameDatabaseSchema::RoundMode>(ee_round);
		entry.eeClampMode = static_cast<GameDatabaseSchema::ClampMode>(ee_clamp);
		entry.vuRoundMode = static_cast<GameDatabaseSchema::RoundMode>(vu_round);
		entry.vuClampMode = static_cast<GameDatabaseSchema::ClampMode>(vu_clamp);

		entry.gameFixes.resize(game_fix_count);
		for (u32 i = 0; i < game_fix_count; i++)
		{
			if (!ReadString(fp.get(), &entry.gameFixes[i]))
				return false;
		}

		for (u32 i = 0; i < speed_hack_count; i++)
		{
			std::string speed_hack_name;
			s32 speed_hack_value;
			if (!ReadString(fp.get(), &speed_hack_name) || !ReadS32(fp.get(), &speed_hack_value))
				return false;
			entry.speedHacks.emplace(std::move(speed_hack_name), speed_hack_value);
		}

		entry.memcardFilters.resize(memcard_filter_count);
		for (u32 i = 0; i < memcard_filter_count; i++)
		{
			if (!ReadString(fp.get(), &entry.memcardFilters[i]))
				return false;
		}

		for (u32 i = 0; i < patch_count; i++)
		{
			std::string patch_crc;
			u32 patch_line_count;
			if (!ReadString(fp.get(), &patch_crc) || !ReadU32(fp.get(), &patch_line_count))
				return false;

			GameDatabaseSchema::Patch patch_lines;
			patch_lines.resize(patch_line_count);
			for (u32 j = 0; j < patch_line_count; j++)
			{
				if (!ReadString(fp.get(), &patch_lines[j]))
					return false;
			}

			entry.patches.emplace(std::move(patch_crc), std::move(patch_lines));
		}

		s_game_db.emplace(std::move(serial), std::move(entry));
	}

	return true;
}

static bool SaveCache(const char* cached_filename, s64 mtime)
{
	auto fp = FileSystem::OpenManagedCFile(cached_filename, "wb");
	if (!fp)
		return false;

	if (!WriteU64(fp.get(), CACHE_FILE_MAGIC) || !WriteS64(fp.get(), mtime))
		return false;

	for (const auto& it : s_game_db)
	{
		const GameDatabaseSchema::GameEntry& entry = it.second;
		const u8 compat = static_cast<u8>(entry.compat);
		const s8 ee_round = static_cast<s8>(entry.eeRoundMode);
		const s8 ee_clamp = static_cast<s8>(entry.eeClampMode);
		const s8 vu_round = static_cast<s8>(entry.vuRoundMode);
		const s8 vu_clamp = static_cast<s8>(entry.vuClampMode);

		if (!WriteString(fp.get(), it.first) ||
			!WriteString(fp.get(), entry.name) ||
			!WriteString(fp.get(), entry.region) ||
			!WriteU8(fp.get(), compat) ||
			!WriteS8(fp.get(), ee_round) ||
			!WriteS8(fp.get(), ee_clamp) ||
			!WriteS8(fp.get(), vu_round) ||
			!WriteS8(fp.get(), vu_clamp) ||
			!WriteU32(fp.get(), static_cast<u32>(entry.gameFixes.size())) ||
			!WriteU32(fp.get(), static_cast<u32>(entry.speedHacks.size())) ||
			!WriteU32(fp.get(), static_cast<u32>(entry.memcardFilters.size())) ||
			!WriteU32(fp.get(), static_cast<u32>(entry.patches.size())))
		{
			return false;
		}

		for (const std::string& it : entry.gameFixes)
		{
			if (!WriteString(fp.get(), it))
				return false;
		}

		for (const auto& it : entry.speedHacks)
		{
			if (!WriteString(fp.get(), it.first) || !WriteS32(fp.get(), it.second))
				return false;
		}

		for (const std::string& it : entry.memcardFilters)
		{
			if (!WriteString(fp.get(), it))
				return false;
		}

		for (const auto& it : entry.patches)
		{
			if (!WriteString(fp.get(), it.first) || !WriteU32(fp.get(), static_cast<u32>(it.second.size())))
				return false;

			for (const std::string& jt : it.second)
			{
				if (!WriteString(fp.get(), jt))
					return false;
			}
		}
	}

	return std::fflush(fp.get()) == 0;
}

static void Load()
{
	const std::string cache_filename(Path::CombineStdString(EmuFolders::Cache, GAMEDB_CACHE_FILE_NAME));
	const s64 expected_mtime = GetExpectedMTime();

	Common::Timer timer;

	if (!FileSystem::FileExists(cache_filename.c_str()) || !CheckAndLoad(cache_filename.c_str(), expected_mtime))
	{
		Console.Warning("GameDB cache file does not exist or failed validation, recreating");
		s_game_db.clear();

		if (!LoadYamlFile())
		{
			Console.Error("GameDB: Failed to load YAML file");
			return;
		}

		if (!SaveCache(cache_filename.c_str(), expected_mtime))
			Console.Error("GameDB: Failed to save new cache");
	}

	Console.WriteLn("[GameDB] %zu games on record (loaded in %.2fms)", s_game_db.size(), timer.GetTimeMilliseconds());
}

void GameDatabase::EnsureLoaded()
{
	std::call_once(s_load_once_flag, []() {
		Load();
	});
}

const GameDatabaseSchema::GameEntry* GameDatabase::FindGame(const std::string& serial)
{
	EnsureLoaded();

	const std::string serialLower(strToLower(serial));
	Console.WriteLn("[GameDB] Searching for '%s' in GameDB", serialLower.c_str());

	auto iter = s_game_db.find(serialLower);
	if (iter == s_game_db.end())
	{
		Console.Error("[GameDB] Could not find '%s' in GameDB", serialLower.c_str());
		return nullptr;
	}

	Console.WriteLn("[GameDB] Found '%s' in GameDB", serialLower.c_str());
	return &iter->second;
}
