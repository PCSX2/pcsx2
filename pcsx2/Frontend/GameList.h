/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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
#include "GameDatabase.h"
#include "common/Pcsx2Defs.h"
#include <ctime>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class ProgressCallback;

struct VMBootParameters;

namespace GameList
{
	enum class EntryType
	{
		PS2Disc,
		PS1Disc,
		ELF,
		Playlist,
		Count
	};

	enum class Region
	{
		NTSC_B,
		NTSC_C,
		NTSC_HK,
		NTSC_J,
		NTSC_K,
		NTSC_T,
		NTSC_U,
		Other,
		PAL_A,
		PAL_AU,
		PAL_AF,
		PAL_BE,
		PAL_E,
		PAL_F,
		PAL_FI,
		PAL_G,
		PAL_GR,
		PAL_I,
		PAL_IN,
		PAL_M,
		PAL_NL,
		PAL_NO,
		PAL_P,
		PAL_R,
		PAL_S,
		PAL_SC,
		PAL_SW,
		PAL_SWI,
		PAL_UK,
		Count
	};

	using CompatibilityRating = GameDatabaseSchema::Compatibility;
	static constexpr u32 CompatibilityRatingCount = static_cast<u32>(GameDatabaseSchema::Compatibility::Perfect) + 1u;

	struct Entry
	{
		EntryType type = EntryType::PS2Disc;
		Region region = Region::Other;

		std::string path;
		std::string serial;
		std::string title;
		u64 total_size = 0;
		std::time_t last_modified_time = 0;

		u32 crc = 0;

		CompatibilityRating compatibility_rating = CompatibilityRating::Unknown;
	};

	const char* EntryTypeToString(EntryType type);
	const char* RegionToString(Region region);
	const char* EntryCompatibilityRatingToString(CompatibilityRating rating);

	bool IsScannableFilename(const std::string_view& path);

	/// Fills in boot parameters (iso or elf) based on the game list entry.
	void FillBootParametersForEntry(VMBootParameters* params, const Entry* entry);

	/// Populates a game list entry struct with information from the iso/elf.
	/// Do *not* call while the system is running, it will mess with CDVD state.
	bool PopulateEntryFromPath(const std::string& path, GameList::Entry* entry);

	// Game list access. It's the caller's responsibility to hold the lock while manipulating the entry in any way.
	std::unique_lock<std::recursive_mutex> GetLock();
	const Entry* GetEntryByIndex(u32 index);
	const Entry* GetEntryForPath(const char* path);
	const Entry* GetEntryByCRC(u32 crc);
	const Entry* GetEntryBySerialAndCRC(const std::string_view& serial, u32 crc);
	u32 GetEntryCount();

	bool IsGameListLoaded();
	void Refresh(bool invalidate_cache, ProgressCallback* progress = nullptr);

	std::string GetCoverImagePathForEntry(const Entry* entry);
	std::string GetCoverImagePath(const std::string& path, const std::string& code, const std::string& title);
	std::string GetNewCoverImagePathForEntry(const Entry* entry, const char* new_filename);
} // namespace GameList
