// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ImGuiManager.h"
#include "Config.h"

struct ImVec2;

namespace ImGuiManager
{
	void RenderOverlays();
}

ImVec2 CalculateOSDPosition(OsdOverlayPos position, float margin, const ImVec2& text_size, float window_width, float window_height);
ImVec2 CalculatePerformanceOverlayTextPosition(OsdOverlayPos position, float margin, const ImVec2& text_size, float window_width, float position_y);
bool ShouldUseLeftAlignment(OsdOverlayPos position);

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
	void LoadCurrentBackupSlot();
	void SaveCurrentSlot();
} // namespace SaveStateSelectorUI

namespace InputRecordingUI
{
	struct InputRecordingData
	{
		bool is_recording = false;
		TinyString recording_active_message = "";
		TinyString frame_data_message = "";
		TinyString undo_count_message = "";
	};
}

extern InputRecordingUI::InputRecordingData g_InputRecordingData;
