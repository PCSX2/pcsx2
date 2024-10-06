// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ImGuiManager.h"

namespace ImGuiManager
{
	void RenderOverlays();
}

namespace SaveStateSelectorUI
{
	static constexpr float DEFAULT_OPEN_TIME = 5.0f;

	void Open(float open_time = DEFAULT_OPEN_TIME);
	void RefreshList(const std::string& serial, u32 crc);
	void DestroyTextures();
	void Clear();
	void Close();
	bool IsOpen();

	void SelectNextSlot(bool open_selector);
	void SelectPreviousSlot(bool open_selector);

	s32 GetCurrentSlot();
	void LoadCurrentSlot();
	void SaveCurrentSlot();
} // namespace SaveStateSelectorUI
