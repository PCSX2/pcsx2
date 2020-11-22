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

#include "fmt/core.h"
#include "yaml-cpp/yaml.h"
#include <fstream>

std::string GameDatabaseSchema::GameEntry::memcardFiltersAsString()
{
	if (memcardFilters.empty())
		return "";

	std::string filters;
	for (u32 i = 0; i < memcardFilters.size(); i++)
	{
		std::string f = memcardFilters.at(i);
		filters.append(f);
		if (i != memcardFilters.size() - 1)
			filters.append(",");
	}
	return filters;
}

std::vector<std::string> YamlGameDatabaseImpl::convertMultiLineStringToVector(const std::string multiLineString)
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

GameDatabaseSchema::GameEntry YamlGameDatabaseImpl::entryFromYaml(const std::string serial, const YAML::Node& node)
{
	GameDatabaseSchema::GameEntry gameEntry;
	try
	{
		gameEntry.name = node["name"].as<std::string>("");
		gameEntry.region = node["region"].as<std::string>("");
		gameEntry.compat = static_cast<GameDatabaseSchema::Compatibility>(node["compat"].as<int>(enum_cast(gameEntry.compat)));
		gameEntry.eeRoundMode = static_cast<GameDatabaseSchema::RoundMode>(node["eeRoundMode"].as<int>(enum_cast(gameEntry.eeRoundMode)));
		gameEntry.vuRoundMode = static_cast<GameDatabaseSchema::RoundMode>(node["vuRoundMode"].as<int>(enum_cast(gameEntry.vuRoundMode)));
		gameEntry.eeClampMode = static_cast<GameDatabaseSchema::ClampMode>(node["eeClampMode"].as<int>(enum_cast(gameEntry.eeClampMode)));
		gameEntry.vuClampMode = static_cast<GameDatabaseSchema::ClampMode>(node["vuClampMode"].as<int>(enum_cast(gameEntry.vuClampMode)));

		// Validate game fixes, invalid ones will be dropped!
		for (std::string& fix : node["gameFixes"].as<std::vector<std::string>>(std::vector<std::string>()))
		{
			bool fixValidated = false;
			for (GamefixId id = GamefixId_FIRST; id < pxEnumEnd; ++id)
			{
				std::string validFix = wxString(EnumToString(id)).Append(L"Hack").ToStdString();
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

		if (YAML::Node speedHacksNode = node["speedHacks"])
		{
			for (const auto& entry : speedHacksNode)
			{
				// Validate speedhacks, invalid ones will be skipped!
				std::string speedHack = entry.first.as<std::string>();

				// NOTE - currently only 1 speedhack!
				if (speedHack != "mvuFlagSpeedHack")
				{
					Console.Error(fmt::format("[GameDB] Invalid speedhack: '{}', specified for serial: '{}'. Dropping!", speedHack, serial));
					continue;
				}

				gameEntry.speedHacks[speedHack] = entry.second.as<int>();
			}
		}

		gameEntry.memcardFilters = node["memcardFilters"].as<std::vector<std::string>>(std::vector<std::string>());

		if (YAML::Node patches = node["patches"])
		{
			for (const auto& entry : patches)
			{
				std::string crc = entry.first.as<std::string>();
				YAML::Node patchNode = entry.second;

				GameDatabaseSchema::Patch patchCol;

				patchCol.author = patchNode["author"].as<std::string>("");
				patchCol.patchLines = convertMultiLineStringToVector(patchNode["content"].as<std::string>(""));
				gameEntry.patches[crc] = patchCol;
			}
		}
	}
	catch (const YAML::RepresentationException& e)
	{
		Console.Error(fmt::format("[GameDB] Invalid GameDB syntax detected on serial: '{}'.  Error Details - {}", serial, e.msg));
		gameEntry.isValid = false;
	}
	catch (const std::exception& e)
	{
		Console.Error(fmt::format("[GameDB] Unexpected error occurred when reading serial: '{}'.  Error Details - {}", serial, e.what()));
		gameEntry.isValid = false;
	}
	return gameEntry;
}

GameDatabaseSchema::GameEntry YamlGameDatabaseImpl::findGame(const std::string serial)
{
	if (gameDb.count(serial) == 1)
		return gameDb[serial];

	GameDatabaseSchema::GameEntry entry;
	entry.isValid = false;
	return entry;
}

int YamlGameDatabaseImpl::numGames()
{
	return gameDb.size();
}

bool YamlGameDatabaseImpl::initDatabase(std::ifstream& stream)
{
	try
	{
		if (!stream)
		{
			Console.Error("[GameDB] Unable to open GameDB file.");
			return false;
		}
		// yaml-cpp has memory leak issues if you persist and modify a YAML::Node
		// convert to a map and throw it away instead!
		YAML::Node data = YAML::Load(stream);
		for (const auto& entry : data)
		{
			// we don't want to throw away the entire GameDB file if a single entry is made incorrectly,
			// but we do want to yell about it so it can be corrected
			try
			{
				std::string serial = entry.first.as<std::string>();
				gameDb[serial] = entryFromYaml(serial, entry.second);
			}
			catch (const YAML::RepresentationException& e)
			{
				Console.Error(fmt::format("[GameDB] Invalid GameDB syntax detected.  Error Details - {}", e.msg));
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
