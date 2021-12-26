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

#include <sstream>
#include "ryml_std.hpp"
#include "ryml.hpp"
#include "fmt/core.h"
#include "fmt/ranges.h"
#include <fstream>
#include <mutex>

static constexpr char GAMEDB_YAML_FILE_NAME[] = "GameIndex.yaml";

static std::unordered_map<std::string, GameDatabaseSchema::GameEntry> s_game_db;
static std::once_flag s_load_once_flag;

std::string GameDatabaseSchema::GameEntry::memcardFiltersAsString() const
{
	return fmt::to_string(fmt::join(memcardFilters, "/"));
}

const GameDatabaseSchema::Patch* GameDatabaseSchema::GameEntry::findPatch(const std::string_view& crc) const
{
	std::string crcLower = StringUtil::toLower(crc);
	Console.WriteLn(fmt::format("[GameDB] Searching for patch with CRC '{}'", crc));

	auto it = patches.find(crcLower);
	if (it != patches.end())
	{
		Console.WriteLn(fmt::format("[GameDB] Found patch with CRC '{}'", crc));
		return &patches.at(crcLower);
	}

	it = patches.find("default");
	if (it != patches.end())
	{
		Console.WriteLn("[GameDB] Found and falling back to default patch");
		return &patches.at("default");
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

void parseAndInsert(const std::string_view& serial, const c4::yml::NodeRef& node)
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
			auto crc = StringUtil::toLower(std::string(n.key().str, n.key().len));
			if (gameEntry.patches.count(crc) == 1)
			{
				Console.Error(fmt::format("[GameDB] Duplicate CRC '{}' found for serial: '{}'. Skipping, CRCs are case-insensitive!", crc, serial));
				continue;
			}
			GameDatabaseSchema::Patch patch;
			if (n.has_child("content"))
			{
				std::string patchLines;
				n["content"] >> patchLines;
				patch = StringUtil::splitOnNewLine(patchLines);
			}
			gameEntry.patches[crc] = patch;
		}
	}

	s_game_db.emplace(std::move(serial), std::move(gameEntry));
}

static std::ifstream getFileStream(std::string path)
{
#ifdef _WIN32
	return std::ifstream(StringUtil::UTF8StringToWideString(path));
#else
	return std::ifstream(path.c_str());
#endif
}

static void initDatabase()
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
		std::optional<std::vector<u8>> buf(Host::ReadResourceFile(GAMEDB_YAML_FILE_NAME));
		if (!buf.has_value())
		{
			Console.Error("[GameDB] Unable to open GameDB file, file does not exist.");
			return;
		}

		const ryml::substr view = c4::basic_substring<char>(reinterpret_cast<char*>(buf->data()), buf->size());
		ryml::Tree tree = ryml::parse(view);
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
