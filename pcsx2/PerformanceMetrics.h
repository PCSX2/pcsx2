// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

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

	class AverageFPS
	{
	private:
		static constexpr size_t FPS_BUFFER_SIZE = 16;
		std::array<float, FPS_BUFFER_SIZE> fps_buff{};
		size_t count = 0;
		size_t pos = 0;
		float avg_fps = 0.0f;

	public:
		void ClearStats()
		{
			count = 0;
			pos = 0;
			fps_buff.fill(0.0f);
			avg_fps = 0.0f;
		}

		void UpdateAvgFPS(float new_fps_val)
		{
			// If the change is quite dramatic, the user has either turned off the frame limiter or the game is dying.
			// Clear it to catch it up quickly.
			if (count > 0)
			{
				const float last_fps_val = fps_buff[(pos - 1) & (FPS_BUFFER_SIZE - 1)];
				if (std::abs(last_fps_val - new_fps_val) > last_fps_val * 0.20f)
					ClearStats();
			}

			fps_buff[pos] = new_fps_val;
			pos = (pos + 1) & (FPS_BUFFER_SIZE - 1); // if you use values which don't become all 1's, make it %.
			count = std::min(count + 1, FPS_BUFFER_SIZE);

			float avg_sum = 0.0f;

			for (size_t i = 0; i < count; i++)
			{
				avg_sum += fps_buff[i];
			}

			avg_fps = avg_sum / static_cast<float>(count);
		}

		float GetAvgFPS()
		{
			return avg_fps;
		}
	};

	static constexpr u32 NUM_FRAME_TIME_SAMPLES = 150;
	using FrameTimeHistory = std::array<float, NUM_FRAME_TIME_SAMPLES>;

	void Clear();
	void Reset();
	void Update(bool gs_register_write, bool fb_blit, bool is_skipping_present);
	void OnGPUPresent(float gpu_time, u64 vs_invocations, u64 ps_invocations);

	/// Sets the EE thread for CPU usage calculations.
	void SetCPUThread(Threading::ThreadHandle thread);

	/// Sets timers for GS software threads.
	void SetGSSWThreadCount(u32 count);
	void SetGSSWThread(u32 index, Threading::ThreadHandle thread);

	u64 GetFrameNumber();

	InternalFPSMethod GetInternalFPSMethod();
	bool IsInternalFPSValid();

	float GetFPS();
	float GetAvgVPS();
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
	double GetGPUAverageVSInvocations();
	double GetGPUAveragePSInvocations();

	const FrameTimeHistory& GetFrameTimeHistory();
	u32 GetFrameTimeHistoryPos();

	struct SavedMetrics
	{
		float num_samples;
		float frames;
		float time;
		float fps;
		float internal_fps;
		float cpu_thread_usage;
		float cpu_thread_time;
		float gs_thread_usage;
		float gs_thread_time;
		float gpu_time;
		float gpu_usage;
	};

	void StartSavingMetrics(u32 seconds);
	bool IsSavingMetrics();
	void DumpSavedMetrics();
} // namespace PerformanceMetrics
