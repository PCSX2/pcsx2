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

#pragma once

#include "Config.h"
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

enum GamefixId;
enum SpeedhackId;

namespace GameDatabaseSchema
{
	enum class Compatibility
	{
		Unknown = 0,
		Nothing,
		Intro,
		Menu,
		InGame,
		Playable,
		Perfect
	};

	enum class RoundMode
	{
		Undefined = -1,
		Nearest = 0,
		NegativeInfinity,
		PositiveInfinity,
		ChopZero
	};

	enum class ClampMode
	{
		Undefined = -1,
		Disabled = 0,
		Normal,
		Extra,
		Full
	};

	enum class GSHWFixId : u32
	{
		// boolean settings
		AutoFlush,
		ConservativeFramebuffer,
		CPUFramebufferConversion,
		DisableDepthSupport,
		WrapGSMem,
		PreloadFrameData,
		DisablePartialInvalidation,
		TextureInsideRT,
		AlignSprite,
		MergeSprite,
		WildArmsHack,
		PointListPalette,

		// integer settings
		Mipmap,
		TrilinearFiltering,
		SkipDrawStart,
		SkipDrawEnd,
		HalfBottomOverride,
		HalfPixelOffset,
		RoundSprite,
		TexturePreloading,
		Deinterlace,

		Count
	};

	struct GameEntry
	{
		std::string name;
		std::string region;
		Compatibility compat = Compatibility::Unknown;
		RoundMode eeRoundMode = RoundMode::Undefined;
		RoundMode vuRoundMode = RoundMode::Undefined;
		ClampMode eeClampMode = ClampMode::Undefined;
		ClampMode vuClampMode = ClampMode::Undefined;
		std::vector<GamefixId> gameFixes;
		std::vector<std::pair<SpeedhackId, int>> speedHacks;
		std::vector<std::pair<GSHWFixId, s32>> gsHWFixes;
		std::vector<std::string> memcardFilters;
		std::unordered_map<u32, std::string> patches;

		// Returns the list of memory card serials as a `/` delimited string
		std::string memcardFiltersAsString() const;
		const std::string* findPatch(u32 crc) const;
		const char* compatAsString() const;

		/// Applies Core game fixes to an existing config. Returns the number of applied fixes.
		u32 applyGameFixes(Pcsx2Config& config, bool applyAuto) const;

		/// Applies GS hardware fixes to an existing config. Returns the number of applied fixes.
		u32 applyGSHardwareFixes(Pcsx2Config::GSOptions& config) const;
	};
};

namespace GameDatabase
{
	void ensureLoaded();
	const GameDatabaseSchema::GameEntry* findGame(const std::string_view& serial);
}; // namespace GameDatabase
