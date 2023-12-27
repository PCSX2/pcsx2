// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <array>
#include "common/Threading.h"

namespace PerformanceMetrics
{
	enum class InternalFPSMethod
	{
		None,
		GSPrivilegedRegister,
		DISPFBBlit
	};

	static constexpr u32 NUM_FRAME_TIME_SAMPLES = 150;
	using FrameTimeHistory = std::array<float, NUM_FRAME_TIME_SAMPLES>;

	void Clear();
	void Reset();
	void Update(bool gs_register_write, bool fb_blit, bool is_skipping_present);
	void OnGPUPresent(float gpu_time);

	/// Sets the EE thread for CPU usage calculations.
	void SetCPUThread(Threading::ThreadHandle thread);

	/// Sets timers for GS software threads.
	void SetGSSWThreadCount(u32 count);
	void SetGSSWThread(u32 index, Threading::ThreadHandle thread);

	u64 GetFrameNumber();

	InternalFPSMethod GetInternalFPSMethod();
	bool IsInternalFPSValid();

	float GetFPS();
	float GetInternalFPS();
	float GetSpeed();
	float GetAverageFrameTime();
	float GetMinimumFrameTime();
	float GetMaximumFrameTime();

	double GetCPUThreadUsage();
	double GetCPUThreadAverageTime();
	float GetGSThreadUsage();
	float GetGSThreadAverageTime();
	float GetVUThreadUsage();
	float GetVUThreadAverageTime();
	float GetCaptureThreadUsage();
	float GetCaptureThreadAverageTime();

	u32 GetGSSWThreadCount();
	double GetGSSWThreadUsage(u32 index);
	double GetGSSWThreadAverageTime(u32 index);

	float GetGPUUsage();
	float GetGPUAverageTime();

	const FrameTimeHistory& GetFrameTimeHistory();
	u32 GetFrameTimeHistoryPos();
} // namespace PerformanceMetrics
