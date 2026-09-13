// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <chrono>
#include <vector>
#include <cmath>

#include "common/Timer.h"
#include "common/Threading.h"

#include "PerformanceMetrics.h"

#include "GS.h"
#include "GS/GSCapture.h"
#include "MTGS.h"
#include "MTVU.h"
#include "VMManager.h"

namespace PerformanceMetrics
{
	static const float UPDATE_INTERVAL = 0.5f;

	static float s_fps = 0.0f;
	static AvgFPS s_avg_vps;
	static float s_internal_fps = 0.0f;
	static float s_minimum_frame_time = 0.0f;
	static float s_minimum_frame_time_accumulator = 0.0f;
	static float s_average_frame_time = 0.0f;
	static float s_average_frame_time_accumulator = 0.0f;
	static float s_maximum_frame_time = 0.0f;
	static float s_maximum_frame_time_accumulator = 0.0f;
	static u32 s_frames_since_last_update = 0;
	static u32 s_unskipped_frames_since_last_update = 0;
	static Common::Timer s_last_update_time;
	static Common::Timer s_last_frame_time;

	// frame number, updated by the GS thread
	static u64 s_frame_number = 0;

	// internal fps heuristics
	static InternalFPSMethod s_internal_fps_method = InternalFPSMethod::None;
	static u32 s_gs_framebuffer_blits_since_last_update = 0;
	static u32 s_gs_privileged_register_writes_since_last_update = 0;

	static Threading::ThreadHandle s_cpu_thread_handle;
	static u64 s_last_cpu_time = 0;
	static u64 s_last_gs_time = 0;
	static u64 s_last_vu_time = 0;
	static u64 s_last_capture_time = 0;
	static u64 s_last_ticks = 0;

	static double s_cpu_thread_usage = 0.0f;
	static double s_cpu_thread_time = 0.0f;
	static float s_gs_thread_usage = 0.0f;
	static float s_gs_thread_time = 0.0f;
	static float s_vu_thread_usage = 0.0f;
	static float s_vu_thread_time = 0.0f;
	static float s_capture_thread_usage = 0.0f;
	static float s_capture_thread_time = 0.0f;

	static FrameTimeHistory s_frame_time_history;
	static u32 s_frame_time_history_pos = 0;

	struct GSSWThreadStats
	{
		Threading::ThreadHandle handle;
		u64 last_cpu_time = 0;
		double usage = 0.0;
		double time = 0.0;
	};
	std::vector<GSSWThreadStats> s_gs_sw_threads;

	static float s_average_gpu_time = 0.0f;
	static float s_accumulated_gpu_time = 0.0f;
	static float s_gpu_usage = 0.0f;
	static u32 s_presents_since_last_update = 0;
	static double s_average_gpu_vs_invocations = 0.0;
	static double s_average_gpu_ps_invocations = 0.0;
	static u64 s_accumulated_gpu_vs_invocations = 0;
	static u64 s_accumulated_gpu_ps_invocations = 0;

	static SavedMetrics s_saved_metrics;
	static SavedMetrics s_saved_metrics_std;
	static float s_saved_metrics_remaining_seconds = 0.0f;

	void Clear()
	{
		Reset();

		s_fps = 0.0f;
		s_avg_vps.ClearStats();
		s_internal_fps = 0.0f;
		s_minimum_frame_time = 0.0f;
		s_average_frame_time = 0.0f;
		s_maximum_frame_time = 0.0f;
		s_internal_fps_method = InternalFPSMethod::None;
		s_average_gpu_vs_invocations = 0.0;
		s_average_gpu_ps_invocations = 0.0;

		s_cpu_thread_usage = 0.0f;
		s_cpu_thread_time = 0.0f;
		s_gs_thread_usage = 0.0f;
		s_gs_thread_time = 0.0f;
		s_vu_thread_usage = 0.0f;
		s_vu_thread_time = 0.0f;
		s_capture_thread_usage = 0.0f;
		s_capture_thread_time = 0.0f;

		s_average_gpu_time = 0.0f;
		s_gpu_usage = 0.0f;

		s_frame_number = 0;

		s_frame_time_history.fill(0.0f);
		s_frame_time_history_pos = 0;

		s_saved_metrics = {};
		s_saved_metrics_std = {};
		s_saved_metrics_remaining_seconds = 0.0f;
	}

	void Reset()
	{
		s_frames_since_last_update = 0;
		s_unskipped_frames_since_last_update = 0;
		s_gs_framebuffer_blits_since_last_update = 0;
		s_gs_privileged_register_writes_since_last_update = 0;
		s_minimum_frame_time_accumulator = 0.0f;
		s_average_frame_time_accumulator = 0.0f;
		s_maximum_frame_time_accumulator = 0.0f;
		s_accumulated_gpu_vs_invocations = 0;
		s_accumulated_gpu_ps_invocations = 0;

		s_accumulated_gpu_time = 0.0f;
		s_presents_since_last_update = 0;

		s_last_update_time.Reset();
		s_last_frame_time.Reset();

		s_last_cpu_time = s_cpu_thread_handle.GetCPUTime();
		s_last_gs_time = MTGS::GetThreadHandle().GetCPUTime();
		s_last_vu_time = THREAD_VU1 ? vu1Thread.GetThreadHandle().GetCPUTime() : 0;
		s_last_ticks = GetCPUTicks();
		s_last_capture_time = GSCapture::IsCapturing() ? GSCapture::GetEncoderThreadHandle().GetCPUTime() : 0;

		for (GSSWThreadStats& stat : s_gs_sw_threads)
			stat.last_cpu_time = stat.handle.GetCPUTime();
	}

	static double GetCPUTimeToPCTFactor(u64 ticks_delta)
	{
		return 100.0 * static_cast<double>(GetTickFrequency()) /
		       (static_cast<double>(ticks_delta) * static_cast<double>(Threading::GetThreadTicksPerSecond()));
	}

	static double GetCPUTimeToMSFactor(u64 frames)
	{
		return 1000.0 / static_cast<double>(Threading::GetThreadTicksPerSecond()) / static_cast<double>(frames);
	}

	void Update(bool gs_register_write, bool fb_blit, bool is_skipping_present)
	{
		if (!is_skipping_present)
		{
			const float frame_time = s_last_frame_time.GetTimeMillisecondsAndReset();
			s_minimum_frame_time_accumulator = (s_minimum_frame_time_accumulator == 0.0f) ? frame_time : std::min(s_minimum_frame_time_accumulator, frame_time);
			s_average_frame_time_accumulator += frame_time;
			s_maximum_frame_time_accumulator = std::max(s_maximum_frame_time_accumulator, frame_time);
			s_frame_time_history[s_frame_time_history_pos] = frame_time;
			s_frame_time_history_pos = (s_frame_time_history_pos + 1) % NUM_FRAME_TIME_SAMPLES;
			s_unskipped_frames_since_last_update++;
		}

		s_frames_since_last_update++;
		s_gs_privileged_register_writes_since_last_update += static_cast<u32>(gs_register_write);
		s_gs_framebuffer_blits_since_last_update += static_cast<u32>(fb_blit);
		s_frame_number++;

		const Common::Timer::Value now_ticks = Common::Timer::GetCurrentValue();
		const Common::Timer::Value ticks_diff = now_ticks - s_last_update_time.GetStartValue();
		const float time = Common::Timer::ConvertValueToSeconds(ticks_diff);
		if (time < UPDATE_INTERVAL)
			return;

		s_last_update_time.ResetTo(now_ticks);
		s_minimum_frame_time = std::exchange(s_minimum_frame_time_accumulator, 0.0f);
		s_average_frame_time = std::exchange(s_average_frame_time_accumulator, 0.0f) / static_cast<float>(s_unskipped_frames_since_last_update);
		s_maximum_frame_time = std::exchange(s_maximum_frame_time_accumulator, 0.0f);
		s_fps = static_cast<float>(s_frames_since_last_update) / time;
		s_avg_vps.UpdateAvgFPS(s_fps);
		s_average_gpu_time = s_accumulated_gpu_time / static_cast<float>(s_unskipped_frames_since_last_update);
		s_average_gpu_vs_invocations = static_cast<double>(s_accumulated_gpu_vs_invocations) / static_cast<double>(s_unskipped_frames_since_last_update);
		s_average_gpu_ps_invocations = static_cast<double>(s_accumulated_gpu_ps_invocations) / static_cast<double>(s_unskipped_frames_since_last_update);
		s_gpu_usage = s_accumulated_gpu_time / (time * 10.0f);
		s_accumulated_gpu_time = 0.0f;
		s_accumulated_gpu_vs_invocations = 0;
		s_accumulated_gpu_ps_invocations = 0;

		// prefer privileged register write based framerate detection, it's less likely to have false positives
		if (s_gs_privileged_register_writes_since_last_update > 0 && !EmuConfig.Gamefixes.BlitInternalFPSHack)
		{
			s_internal_fps = static_cast<float>(s_gs_privileged_register_writes_since_last_update) / time;
			s_internal_fps_method = InternalFPSMethod::GSPrivilegedRegister;
		}
		else if (s_gs_framebuffer_blits_since_last_update > 0)
		{
			s_internal_fps = static_cast<float>(s_gs_framebuffer_blits_since_last_update) / time;
			s_internal_fps_method = InternalFPSMethod::DISPFBBlit;
		}
		else
		{
			s_internal_fps = 0;
			s_internal_fps_method = InternalFPSMethod::None;
		}

		s_gs_privileged_register_writes_since_last_update = 0;
		s_gs_framebuffer_blits_since_last_update = 0;

		const u64 ticks = GetCPUTicks();
		const u64 ticks_delta = ticks - s_last_ticks;
		s_last_ticks = ticks;

		const double pct_divider = GetCPUTimeToPCTFactor(ticks_delta);
		const double time_divider = GetCPUTimeToMSFactor(s_frames_since_last_update);

		const u64 cpu_time = s_cpu_thread_handle.GetCPUTime();
		const u64 gs_time = MTGS::GetThreadHandle().GetCPUTime();
		const u64 vu_time = THREAD_VU1 ? vu1Thread.GetThreadHandle().GetCPUTime() : 0;
		const u64 capture_time = GSCapture::IsCapturing() ? GSCapture::GetEncoderThreadHandle().GetCPUTime() : 0;

		const u64 cpu_delta = cpu_time - s_last_cpu_time;
		const u64 gs_delta = gs_time - s_last_gs_time;
		const u64 vu_delta = vu_time - s_last_vu_time;
		const u64 capture_delta = capture_time - s_last_capture_time;
		s_last_cpu_time = cpu_time;
		s_last_gs_time = gs_time;
		s_last_vu_time = vu_time;
		s_last_capture_time = capture_time;

		s_cpu_thread_usage = static_cast<double>(cpu_delta) * pct_divider;
		s_gs_thread_usage = static_cast<double>(gs_delta) * pct_divider;
		s_vu_thread_usage = static_cast<double>(vu_delta) * pct_divider;
		s_capture_thread_usage = static_cast<double>(capture_delta) * pct_divider;
		s_cpu_thread_time = static_cast<double>(cpu_delta) * time_divider;
		s_gs_thread_time = static_cast<double>(gs_delta) * time_divider;
		s_vu_thread_time = static_cast<double>(vu_delta) * time_divider;
		s_capture_thread_time = static_cast<double>(capture_delta) * time_divider;

		for (GSSWThreadStats& thread : s_gs_sw_threads)
		{
			const u64 time = thread.handle.GetCPUTime();
			const u64 delta = time - thread.last_cpu_time;
			thread.last_cpu_time = time;
			thread.usage = static_cast<double>(delta) * pct_divider;
			thread.time = static_cast<double>(delta) * time_divider;
		}

		if (s_saved_metrics_remaining_seconds > 0.0f)
		{
			auto UpdateSavedMetrics = [&](SavedMetrics& metrics, bool square) {
				auto Transform = [square](float f) { return square ? f * f : f; };

				metrics.num_samples += 1.0f;
				metrics.frames += static_cast<float>(s_frames_since_last_update);
				metrics.time += time;
				metrics.fps += Transform(s_fps);
				metrics.internal_fps += Transform(s_internal_fps);
				metrics.cpu_thread_usage += Transform(s_cpu_thread_usage);
				metrics.cpu_thread_time += Transform(s_cpu_thread_time);
				metrics.gs_thread_usage += Transform(s_gs_thread_usage);
				metrics.gs_thread_time += Transform(s_gs_thread_time);
				metrics.gpu_time += Transform(s_average_gpu_time);
				;
				metrics.gpu_usage += Transform(s_gpu_usage);
			};

			UpdateSavedMetrics(s_saved_metrics, false);
			UpdateSavedMetrics(s_saved_metrics_std, true);

			s_saved_metrics_remaining_seconds -= std::min(s_saved_metrics_remaining_seconds, time);

			std::atomic_thread_fence(std::memory_order_release); // results might be read on another thread
		}

		s_frames_since_last_update = 0;
		s_unskipped_frames_since_last_update = 0;
		s_presents_since_last_update = 0;

		Host::OnPerformanceMetricsUpdated();
	}

	void OnGPUPresent(float gpu_time, u64 vs_invocations, u64 ps_invocations)
	{
		s_accumulated_gpu_time += gpu_time;
		s_accumulated_gpu_vs_invocations += vs_invocations;
		s_accumulated_gpu_ps_invocations += ps_invocations;
		s_presents_since_last_update++;
	}

	void SetCPUThread(Threading::ThreadHandle thread)
	{
		s_last_cpu_time = thread ? thread.GetCPUTime() : 0;
		s_cpu_thread_handle = std::move(thread);
	}

	void SetGSSWThreadCount(u32 count)
	{
		s_gs_sw_threads.clear();
		s_gs_sw_threads.resize(count);
	}

	void SetGSSWThread(u32 index, Threading::ThreadHandle thread)
	{
		s_gs_sw_threads[index].last_cpu_time = thread ? thread.GetCPUTime() : 0;
		s_gs_sw_threads[index].handle = std::move(thread);
	}

	u64 GetFrameNumber()
	{
		return s_frame_number;
	}

	InternalFPSMethod GetInternalFPSMethod()
	{
		return s_internal_fps_method;
	}

	bool IsInternalFPSValid()
	{
		return s_internal_fps_method != InternalFPSMethod::None;
	}

	float GetFPS()
	{
		return s_fps;
	}

	float GetAvgVPS()
	{
		return s_avg_vps.GetAvgFPS();
	}

	float GetInternalFPS()
	{
		return s_internal_fps;
	}

	float GetSpeed()
	{
		return (s_fps / VMManager::GetFrameRate()) * 100.0;
	}

	float GetAverageFrameTime()
	{
		return s_average_frame_time;
	}

	float GetMinimumFrameTime()
	{
		return s_minimum_frame_time;
	}

	float GetMaximumFrameTime()
	{
		return s_maximum_frame_time;
	}

	double GetCPUThreadUsage()
	{
		return s_cpu_thread_usage;
	}

	double GetCPUThreadAverageTime()
	{
		return s_cpu_thread_time;
	}

	float GetGSThreadUsage()
	{
		return s_gs_thread_usage;
	}

	float GetGSThreadAverageTime()
	{
		return s_gs_thread_time;
	}

	float GetVUThreadUsage()
	{
		return s_vu_thread_usage;
	}

	float GetVUThreadAverageTime()
	{
		return s_vu_thread_time;
	}

	float GetCaptureThreadUsage()
	{
		return s_capture_thread_usage;
	}

	float GetCaptureThreadAverageTime()
	{
		return s_capture_thread_time;
	}

	u32 GetGSSWThreadCount()
	{
		return static_cast<u32>(s_gs_sw_threads.size());
	}

	double GetGSSWThreadUsage(u32 index)
	{
		return s_gs_sw_threads[index].usage;
	}

	double GetGSSWThreadAverageTime(u32 index)
	{
		return s_gs_sw_threads[index].time;
	}

	float GetGPUUsage()
	{
		return s_gpu_usage;
	}

	float GetGPUAverageTime()
	{
		return s_average_gpu_time;
	}

	double GetGPUAverageVSInvocations()
	{
		return s_average_gpu_vs_invocations;
	}

	double GetGPUAveragePSInvocations()
	{
		return s_average_gpu_ps_invocations;
	}

	const FrameTimeHistory& GetFrameTimeHistory()
	{
		return s_frame_time_history;
	}

	u32 GetFrameTimeHistoryPos()
	{
		return s_frame_time_history_pos;
	}

	void StartSavingMetrics(u32 seconds)
	{
		s_saved_metrics_remaining_seconds = static_cast<float>(seconds);
		s_saved_metrics = {};
		s_saved_metrics_std = {};
	}

	bool IsSavingMetrics()
	{
		return s_saved_metrics_remaining_seconds > 0.0f;
	}

	void DumpSavedMetrics()
	{
		s_saved_metrics_remaining_seconds = 0.0f;

		SavedMetrics metrics = std::exchange(s_saved_metrics, SavedMetrics{});
		SavedMetrics metrics_std = std::exchange(s_saved_metrics_std, SavedMetrics{});

		const float num_samples = metrics.num_samples;

		if (num_samples > 0.0f)
		{
			const auto Square = [](float f) { return f * f; };
			const auto Average = [num_samples](float f) { return f / num_samples; };

			metrics.fps = Average(metrics.fps);
			metrics.internal_fps = Average(metrics.internal_fps);
			metrics.cpu_thread_usage = Average(metrics.cpu_thread_usage);
			metrics.cpu_thread_time = Average(metrics.cpu_thread_time);
			metrics.gs_thread_usage = Average(metrics.gs_thread_usage);
			metrics.gs_thread_time = Average(metrics.gs_thread_time);
			metrics.gpu_time = Average(metrics.gpu_time);
			metrics.gpu_usage = Average(metrics.gpu_usage);

			metrics_std.fps = std::sqrt((Average(metrics_std.fps) - Square(metrics.fps)));
			metrics_std.internal_fps = std::sqrt((Average(metrics_std.internal_fps) - Square(metrics.internal_fps)));
			metrics_std.cpu_thread_usage = std::sqrt((Average(metrics_std.cpu_thread_usage) - Square(metrics.cpu_thread_usage)));
			metrics_std.cpu_thread_time = std::sqrt((Average(metrics_std.cpu_thread_time) - Square(metrics.cpu_thread_time)));
			metrics_std.gs_thread_usage = std::sqrt((Average(metrics_std.gs_thread_usage) - Square(metrics.gs_thread_usage)));
			metrics_std.gs_thread_time = std::sqrt((Average(metrics_std.gs_thread_time) - Square(metrics.gs_thread_time)));
			metrics_std.gpu_time = std::sqrt((Average(metrics_std.gpu_time) - Square(metrics.gpu_time)));
			metrics_std.gpu_usage = std::sqrt((Average(metrics_std.gpu_usage) - Square(metrics.gpu_usage)));

			Console.WriteLnFmt("@HWSTAT@ Frames: {} ({} samples)", metrics.frames, metrics.num_samples);
			Console.WriteLnFmt("@HWSTAT@ Time: {:.3f} sec", metrics.time);
			Console.WriteLnFmt("@HWSTAT@ FPS: {:.3f} ± {:.3f} ({:.3f} ± {:.3f} internal)", metrics.fps, metrics_std.fps, metrics.internal_fps, metrics_std.internal_fps);
			Console.WriteLnFmt("@HWSTAT@ Minimum Frame Time: {:.3f} ms ({:.3f} FPS)", GetMinimumFrameTime(), 1000.0f / GetMinimumFrameTime());
			Console.WriteLnFmt("@HWSTAT@ Average Frame Time: {:.3f} ms ({:.3f} FPS)", GetAverageFrameTime(), 1000.0f / GetAverageFrameTime());
			Console.WriteLnFmt("@HWSTAT@ Maximum Frame Time: {:.3f} ms ({:.3f} FPS)", GetMaximumFrameTime(), 1000.0f / GetMaximumFrameTime());
			Console.WriteLnFmt("@HWSTAT@ Average CPU Thread Usage: {:.3f} ± {:.3f} %", metrics.cpu_thread_usage, metrics_std.cpu_thread_usage);
			Console.WriteLnFmt("@HWSTAT@ Average GS Thread Usage: {:.3f} ± {:.3f} %", metrics.gs_thread_usage, metrics_std.gs_thread_usage);
			Console.WriteLnFmt("@HWSTAT@ Average GPU Usage: {:.3f} ± {:.3f} %", metrics.gpu_usage, metrics_std.gpu_usage);
			Console.WriteLnFmt("@HWSTAT@ Average CPU Thread Time: {:.3f} ± {:.3f} ms", metrics.cpu_thread_time, metrics_std.cpu_thread_time);
			Console.WriteLnFmt("@HWSTAT@ Average GS Thread Time: {:.3f} ± {:.3f} ms", metrics.gs_thread_time, metrics_std.gs_thread_time);
			Console.WriteLnFmt("@HWSTAT@ Average GPU Time: {:.3f} ± {:.3f} ms", metrics.gpu_time, metrics_std.gpu_time);
		}
	}
}
