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

/// TODO - the following helper functions can realistically be put in some sort of general yaml utility library
// TODO - might be a way to condense this with templates? get it working first
std::string YamlGameDatabaseImpl::safeGetString(const YAML::Node& n, std::string key, std::string def)
{
	if (!n[key])
		return def;
	// TODO - test this safety consideration (parse a value that isn't actually a string
	return n[key].as<std::string>();
}

int YamlGameDatabaseImpl::safeGetInt(const YAML::Node& n, std::string key, int def)
{
	if (!n[key])
		return def;
	// TODO - test this safety consideration (parse a value that isn't actually an int
	return n[key].as<int>();
}

std::vector<std::string> YamlGameDatabaseImpl::safeGetStringList(const YAML::Node& n, std::string key, std::vector<std::string> def)
{
	if (!n[key])
		return def;
	// TODO - test this safety consideration (parse a value that isn't actually a string
	return n[key].as<std::vector<std::string>>();
}

GameDatabaseSchema::GameEntry YamlGameDatabaseImpl::entryFromYaml(const YAML::Node& node)
{
	GameDatabaseSchema::GameEntry entry;
	try
	{
		entry.name = safeGetString(node, "name");
		entry.region = safeGetString(node, "region");
		entry.compat = static_cast<GameDatabaseSchema::Compatibility>(safeGetInt(node, "compat"));
		entry.eeRoundMode = static_cast<GameDatabaseSchema::RoundMode>(safeGetInt(node, "eeRoundMode"));
		entry.vuRoundMode = static_cast<GameDatabaseSchema::RoundMode>(safeGetInt(node, "vuRoundMode"));
		entry.eeClampMode = static_cast<GameDatabaseSchema::ClampMode>(safeGetInt(node, "eeClampMode"));
		entry.vuClampMode = static_cast<GameDatabaseSchema::ClampMode>(safeGetInt(node, "vuClampMode"));
		entry.gameFixes = safeGetStringList(node, "gameFixes");
		entry.speedHacks = safeGetStringList(node, "speedHacks");
		entry.memcardFilters = safeGetStringList(node, "memcardFilters");

		if (YAML::Node patches = node["patches"])
		{
			for (YAML::const_iterator it = patches.begin(); it != patches.end(); ++it)
			{
				YAML::Node key = it->first;
				YAML::Node val = it->second;
				GameDatabaseSchema::PatchCollection patchCol;
				patchCol.author = safeGetString(val, "author");
				patchCol.patchLines = safeGetStringList(val, "patchLines");
				entry.patches[key.as<std::string>()] = patchCol;
			}
		}
	}
	catch (std::exception& e)
	{
		entry.isValid = false;
	}
	return entry;
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

// yaml-cpp definitely takes longer to parse this giant file, but the library feels more mature
// rapidyaml exists and I have it mostly setup in another branch which could be easily dropped in as a replacement if needed
//
// the problem i ran into with rapidyaml is there is a lack of usage/documentation as it's new
// and i didn't like the default way it handles exceptions (seemed to be configurable, but i didn't have much luck)

bool YamlGameDatabaseImpl::initDatabase(const std::string filePath)
{
	try
	{
		// yaml-cpp has memory leak issues, convert to a map and throw it away!
		YAML::Node data = YAML::LoadFile(filePath);
		for (YAML::const_iterator entry = data.begin(); entry != data.end(); entry++)
		{
			// TODO - helper function to throw an error gracefully if an entry is malformed
			std::string serial = entry->first.as<std::string>();
			gameDb[serial] = entryFromYaml(entry->second);
		}
	}
	catch (const std::exception& e)
	{
		// TODO - error log
		return false;
	}

	return true;
}