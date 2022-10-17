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
#include "common/Pcsx2Types.h"
#include <vector>

namespace Achievements
{
#ifdef ENABLE_ACHIEVEMENTS

	// Implemented in Host.
	extern bool OnReset();
	extern void LoadState(const u8* state_data, u32 state_data_size);
	extern std::vector<u8> SaveState();
	extern void GameChanged(u32 crc);

	/// Re-enables hardcode mode if it is enabled in the settings.
	extern bool ResetChallengeMode();

	/// Forces hardcore mode off until next reset.
	extern void DisableChallengeMode();

	/// Prompts the user to disable hardcore mode, if they agree, returns true.
	extern bool ConfirmChallengeModeDisable(const char* trigger);

	/// Returns true if features such as save states should be disabled.
	extern bool ChallengeModeActive();

#else

	// Make noops when compiling without cheevos.
	static inline bool OnReset()
	{
		return true;
	}
	static inline void LoadState(const u8* state_data, u32 state_data_size)
	{
	}
	static inline std::vector<u8> SaveState()
	{
		return {};
	}
	static inline void GameChanged()
	{
	}

	static constexpr inline bool ChallengeModeActive()
	{
		return false;
	}

	static inline bool ResetChallengeMode()
	{
		return false;
	}

	static inline void DisableChallengeMode() {}

	static inline bool ConfirmChallengeModeDisable(const char* trigger)
	{
		return true;
	}

#endif
} // namespace Achievements
