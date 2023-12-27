// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
