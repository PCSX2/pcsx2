// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Error.h"

#include <string>
#include <vector>

namespace GSDumpReplayer
{
	bool IsReplayingDump();

	/// If set, playback will repeat once it reaches the last frame.
	void SetLoopCount(s32 loop_count = 0);
	int GetLoopCount();
	bool IsRunner();
	void SetIsDumpRunner(bool is_runner, bool dump_perf = true);
	void SetFrameRange(bool use, u32 start, u32 end);

	bool Initialize(const char* filename, Error* error = nullptr);
	bool ChangeDump(const char* filename);
	void Shutdown();

	std::string GetDumpSerial();
	u32 GetDumpCRC();

	u32 GetFrameNumber();

	void RenderUI();

	struct PerfMetrics
	{
		float num_updates;
		float fps;
		float internal_fps;
		float cpu_thread_usage;
		float cpu_thread_time;
		float gs_thread_usage;
		float gs_thread_time;
		float gpu_time;
		float gpu_usage;
	};

	void UpdatePerformanceMetrics();

	void UpdateGSStats();

	void DumpStats();
} // namespace GSDumpReplayer
