/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include "ImGuiManager.h"

namespace ImGuiManager
{
	void RenderOverlays();
}

namespace SaveStateSelectorUI
{
	static constexpr float DEFAULT_OPEN_TIME = 7.5f;

	void Open(float open_time = DEFAULT_OPEN_TIME);
	void RefreshList(const std::string& serial, u32 crc);
	void DestroyTextures();
	void Clear();
	void Close();

	void SelectNextSlot(bool open_selector);
	void SelectPreviousSlot(bool open_selector);

	s32 GetCurrentSlot();
	void LoadCurrentSlot();
	void SaveCurrentSlot();
} // namespace SaveStateSelectorUI
