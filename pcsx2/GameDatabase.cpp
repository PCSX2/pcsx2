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
#include "fmt/ranges.h"
#include "yaml-cpp/yaml.h"
#include <fstream>
#include <algorithm>

std::string strToLower(std::string str)
{
	std::transform(str.begin(), str.end(), str.begin(),
				   [](unsigned char c) { return std::tolower(c); });
	return str;
}

bool compareStrNoCase(const std::string str1, const std::string str2)
{
	return std::equal(str1.begin(), str1.end(), str2.begin(),
					  [](char a, char b) {
						  return tolower(a) == tolower(b);
					  });
}

std::string GameDatabaseSchema::GameEntry::memcardFiltersAsString() const
{
	return fmt::to_string(fmt::join(memcardFilters, "/"));
}

bool GameDatabaseSchema::GameEntry::findPatch(const std::string crc, Patch& patch) const
{
	std::string crcLower = strToLower(crc);
	Console.WriteLn(fmt::format("[GameDB] Searching for patch with CRC '{}'", crc));
	if (patches.count(crcLower) == 1)
	{
		Console.WriteLn(fmt::format("[GameDB] Found patch with CRC '{}'", crc));
		patch = patches.at(crcLower);
		return true;
	}
	else if (patches.count("default") == 1)
	{
		Console.WriteLn("[GameDB] Found and falling back to default patch");
		patch = patches.at("default");
		return true;
	}
	Console.WriteLn("[GameDB] No CRC-specific patch or default patch found");
	return false;
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
				std::string validFix = fmt::format("{}Hack", wxString(EnumToString(id)));
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
					std::string validSpeedHack = fmt::format("{}SpeedHack", wxString(EnumToString(id)));
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

				GameDatabaseSchema::Patch patchCol;

				patchCol.author = patchNode["author"].as<std::string>("");
				patchCol.patchLines = convertMultiLineStringToVector(patchNode["content"].as<std::string>(""));
				gameEntry.patches[crc] = patchCol;
			}
		}
	}
	catch (const YAML::RepresentationException& e)
	{
		Console.Error(fmt::format("[GameDB] Invalid GameDB syntax detected on serial: '{}'. Error Details - {}", serial, e.msg));
		gameEntry.isValid = false;
	}
	catch (const std::exception& e)
	{
		Console.Error(fmt::format("[GameDB] Unexpected error occurred when reading serial: '{}'. Error Details - {}", serial, e.what()));
		gameEntry.isValid = false;
	}
	return gameEntry;
}

GameDatabaseSchema::GameEntry YamlGameDatabaseImpl::findGame(const std::string serial)
{
	std::string serialLower = strToLower(serial);
	Console.WriteLn(fmt::format("[GameDB] Searching for '{}' in GameDB", serialLower));
	if (gameDb.count(serialLower) == 1)
	{
		Console.WriteLn(fmt::format("[GameDB] Found '{}' in GameDB", serialLower));
		return gameDb[serialLower];
	}

	Console.Error(fmt::format("[GameDB] Could not find '{}' in GameDB", serialLower));
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
				// Serials and CRCs must be inserted as lower-case, as that is how they are retrieved
				// this is because the application may pass a lowercase CRC or serial along
				//
				// However, YAML's keys are as expected case-sensitive, so we have to explicitly do our own duplicate checking
				std::string serial = strToLower(entry.first.as<std::string>());
				if (gameDb.count(serial) == 1)
				{
					Console.Error(fmt::format("[GameDB] Duplicate serial '{}' found in GameDB. Skipping, Serials are case-insensitive!", serial));
					continue;
				}
				gameDb[serial] = entryFromYaml(serial, entry.second);
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
