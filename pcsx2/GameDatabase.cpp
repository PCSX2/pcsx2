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

#include "yaml-cpp/yaml.h"

std::string GameDatabaseSchema::GameEntry::memcardFiltersAsString()
{
	if (memcardFilters.empty())
		return "";

	std::string filters;
	for (int i = 0; i < memcardFilters.size(); i++)
	{
		std::string f = memcardFilters.at(i);
		filters.append(f);
		if (i != memcardFilters.size() - 1)
			filters.append(",");
	}
	return filters;
}

std::string YamlGameDatabaseImpl::safeGetString(const YAML::Node& n, std::string key, std::string def)
{
	if (!n[key])
		return def;
	return n[key].as<std::string>();
}

int YamlGameDatabaseImpl::safeGetInt(const YAML::Node& n, std::string key, int def)
{
	if (!n[key])
		return def;
	return n[key].as<int>();
}

std::vector<std::string> YamlGameDatabaseImpl::safeGetStringList(const YAML::Node& n, std::string key, std::vector<std::string> def)
{
	if (!n[key])
		return def;
	return n[key].as<std::vector<std::string>>();
}

GameDatabaseSchema::GameEntry YamlGameDatabaseImpl::entryFromYaml(const std::string serial, const YAML::Node& node)
{
	GameDatabaseSchema::GameEntry gameEntry;
	try
	{
		gameEntry.name = safeGetString(node, "name");
		gameEntry.region = safeGetString(node, "region");
		gameEntry.compat = static_cast<GameDatabaseSchema::Compatibility>(safeGetInt(node, "compat"));
		gameEntry.eeRoundMode = static_cast<GameDatabaseSchema::RoundMode>(safeGetInt(node, "eeRoundMode"));
		gameEntry.vuRoundMode = static_cast<GameDatabaseSchema::RoundMode>(safeGetInt(node, "vuRoundMode"));
		gameEntry.eeClampMode = static_cast<GameDatabaseSchema::ClampMode>(safeGetInt(node, "eeClampMode"));
		gameEntry.vuClampMode = static_cast<GameDatabaseSchema::ClampMode>(safeGetInt(node, "vuClampMode"));

		// Validate game fixes, invalid ones will be dropped!
		for (std::string fix : gameEntry.gameFixes)
		{
			std::vector<std::string> rawFixes = safeGetStringList(node, "gameFixes");
			bool fixValidated = false;
			for (GamefixId id = GamefixId_FIRST; id < pxEnumEnd; ++id)
			{
				std::string validFix = wxString(EnumToString(id)) + L"Hack";
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
				Console.Error("[GameDB] Invalid gamefix: '%s', specified for serial: '%s'. Dropping!", fix, serial);
			}
		}
		
		if (YAML::Node speedHacksNode = node["speedHacks"])
		{
			for (YAML::const_iterator entry = speedHacksNode.begin(); entry != speedHacksNode.end(); entry++)
			{
				// Validate speedhacks, invalid ones will be skipped!
				std::string speedHack = entry->first.as<std::string>();

				// NOTE - currently only 1 speedhack!
				if (speedHack != "mvuFlagSpeedHack")
				{
					Console.Error("[GameDB] Invalid speedhack: '%s', specified for serial: '%s'. Dropping!", speedHack, serial);
					continue;
				}

				gameEntry.speedHacks[speedHack] = entry->second.as<int>();
			}
		}

		gameEntry.memcardFilters = safeGetStringList(node, "memcardFilters");

		if (YAML::Node patches = node["patches"])
		{
			for (YAML::const_iterator it = patches.begin(); it != patches.end(); ++it)
			{
				YAML::Node key = it->first;
				YAML::Node val = it->second;
				GameDatabaseSchema::Patch patchCol;
				patchCol.author = safeGetString(val, "author");
				patchCol.patchLines = safeGetStringList(val, "patchLines");
				gameEntry.patches[key.as<std::string>()] = patchCol;
			}
		}
	}
	catch (YAML::RepresentationException e)
	{
		Console.Error("[GameDB] Invalid GameDB syntax detected on serial: '%s'.  Error Details - %s", serial, e.what());
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

bool YamlGameDatabaseImpl::initDatabase(const std::string filePath)
{
	try
	{
		// yaml-cpp has memory leak issues if you persist and modify a YAML::Node
		// convert to a map and throw it away instead!
		YAML::Node data = YAML::LoadFile(filePath);
		for (YAML::const_iterator entry = data.begin(); entry != data.end(); entry++)
		{
			// we don't want to throw away the entire GameDB file if a single entry is made, but we do
			// want to yell about it so it can be corrected
			try
			{
				std::string serial = entry->first.as<std::string>();
				gameDb[serial] = entryFromYaml(serial, entry->second);
			}
			catch (YAML::RepresentationException e)
			{
				Console.Error("[GameDB] Invalid GameDB syntax detected.  Error Details - %s", e.what());
			}
		}
	}
	catch (const std::exception& e)
	{
		Console.Error("Error occured when initializing GameDB: %s", e.what());
		return false;
	}

	return true;
}
