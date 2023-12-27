// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <string>
#include <vector>

namespace GSDumpReplayer
{
	bool IsReplayingDump();

	/// If set, playback will repeat once it reaches the last frame.
	void SetLoopCount(s32 loop_count = 0);
	int GetLoopCount();
	bool IsRunner();
	void SetIsDumpRunner(bool is_runner);

	bool Initialize(const char* filename);
	bool ChangeDump(const char* filename);
	void Shutdown();

	std::string GetDumpSerial();
	u32 GetDumpCRC();

	u32 GetFrameNumber();

	void RenderUI();
} // namespace GSDumpReplayer
