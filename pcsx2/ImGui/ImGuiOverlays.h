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

namespace InputRecordingUI
{
	struct InputRecordingData
	{
		std::mutex data_lock;
		bool is_recording = false;
		std::string filename = "";
		u32 current_frame = 0;
		u32 total_frames = 0;
		u32 frame_count = 0;
		u32 undo_count = 0;
	};

	static void UpdateInputRecordingData(InputRecordingData *ird, bool is_recording, std::string filename, u32 current_frame, u32 total_frames, u32 frame_count, u32 undo_count);
}

extern InputRecordingUI::InputRecordingData g_InputRecordingData;
