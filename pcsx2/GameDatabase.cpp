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

static std::unordered_map<std::string, GameDatabaseSchema::GameEntry> s_game_db;
static std::once_flag s_load_once_flag;

static std::string strToLower(std::string str)
{
	std::transform(str.begin(), str.end(), str.begin(),
		[](unsigned char c) { return std::tolower(c); });
	return str;
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
		if (auto gameFixes = node["gameFixes"])
		{
			gameEntry.gameFixes.reserve(gameFixes.size());
			for (std::string& fix : gameFixes.as<std::vector<std::string>>(std::vector<std::string>()))
			{
				// Enum values don't end with Hack, but gamedb does, so remove it before comparing.
				bool fixValidated = false;
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
					Console.Error("[GameDB] Invalid gamefix: '%s', specified for serial: '%s'. Dropping!", fix.c_str(), serial.c_str());
				}
			}
		}

		// Validate speed hacks, invalid ones will be dropped!
		if (auto speedHacksNode = node["speedHacks"])
		{
			gameEntry.speedHacks.reserve(speedHacksNode.size());
			for (const auto& entry : speedHacksNode)
			{
				bool speedHackValidated = false;
				std::string speedHack(entry.first.as<std::string>());

				// Same deal with SpeedHacks
				if (StringUtil::EndsWith(speedHack, "SpeedHack"))
				{
					speedHack.erase(speedHack.size() - 9);
					for (SpeedhackId id = SpeedhackId_FIRST; id < pxEnumEnd; id++)
					{
						if (speedHack.compare(EnumToString(id)) == 0 &&
								std::none_of(gameEntry.speedHacks.begin(), gameEntry.speedHacks.end(), [id](const auto& it) { return it.first == id; }))
						{
							gameEntry.speedHacks.emplace_back(id, entry.second.as<int>());
							speedHackValidated = true;
							break;
						}
					}
				}

				if (!speedHackValidated)
				{
					Console.Error("[GameDB] Invalid speedhack: '%s', specified for serial: '%s'. Dropping!", speedHack.c_str(), serial.c_str());
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

void GameDatabase::EnsureLoaded()
{
	std::call_once(s_load_once_flag, []() {
		Common::Timer timer;

		if (!LoadYamlFile())
		{
			Console.Error("GameDB: Failed to load YAML file");
			return;
		}

		Console.WriteLn("[GameDB] %zu games on record (loaded in %.2fms)", s_game_db.size(), timer.GetTimeMilliseconds());
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
