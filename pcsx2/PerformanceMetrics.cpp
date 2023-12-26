// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <chrono>
#include <vector>

#include "common/Timer.h"
#include "common/Threading.h"

#include "PerformanceMetrics.h"

#include "GS.h"
#include "GS/GSCapture.h"
#include "MTGS.h"
#include "MTVU.h"
#include "VMManager.h"

static const float UPDATE_INTERVAL = 0.5f;

static float s_fps = 0.0f;
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
static PerformanceMetrics::InternalFPSMethod s_internal_fps_method = PerformanceMetrics::InternalFPSMethod::None;
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

static PerformanceMetrics::FrameTimeHistory s_frame_time_history;
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

void PerformanceMetrics::Clear()
{
	Reset();

	s_fps = 0.0f;
	s_internal_fps = 0.0f;
	s_minimum_frame_time = 0.0f;
	s_average_frame_time = 0.0f;
	s_maximum_frame_time = 0.0f;
	s_internal_fps_method = PerformanceMetrics::InternalFPSMethod::None;

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
}

void PerformanceMetrics::Reset()
{
	s_frames_since_last_update = 0;
	s_unskipped_frames_since_last_update = 0;
	s_gs_framebuffer_blits_since_last_update = 0;
	s_gs_privileged_register_writes_since_last_update = 0;
	s_minimum_frame_time_accumulator = 0.0f;
	s_average_frame_time_accumulator = 0.0f;
	s_maximum_frame_time_accumulator = 0.0f;

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

void PerformanceMetrics::Update(bool gs_register_write, bool fb_blit, bool is_skipping_present)
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
	s_average_gpu_time = s_accumulated_gpu_time / static_cast<float>(s_unskipped_frames_since_last_update);
	s_gpu_usage = s_accumulated_gpu_time / (time * 10.0f);
	s_accumulated_gpu_time = 0.0f;

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

	const double pct_divider =
		100.0 * (1.0 / ((static_cast<double>(ticks_delta) * static_cast<double>(Threading::GetThreadTicksPerSecond())) /
						   static_cast<double>(GetTickFrequency())));
	const double time_divider = 1000.0 * (1.0 / static_cast<double>(Threading::GetThreadTicksPerSecond())) *
								(1.0 / static_cast<double>(s_frames_since_last_update));

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

	s_frames_since_last_update = 0;
	s_unskipped_frames_since_last_update = 0;
	s_presents_since_last_update = 0;

	Host::OnPerformanceMetricsUpdated();
}

void PerformanceMetrics::OnGPUPresent(float gpu_time)
{
	s_accumulated_gpu_time += gpu_time;
	s_presents_since_last_update++;
}

void PerformanceMetrics::SetCPUThread(Threading::ThreadHandle thread)
{
	s_last_cpu_time = thread ? thread.GetCPUTime() : 0;
	s_cpu_thread_handle = std::move(thread);
}

void PerformanceMetrics::SetGSSWThreadCount(u32 count)
{
	s_gs_sw_threads.clear();
	s_gs_sw_threads.resize(count);
}

void PerformanceMetrics::SetGSSWThread(u32 index, Threading::ThreadHandle thread)
{
	s_gs_sw_threads[index].last_cpu_time = thread ? thread.GetCPUTime() : 0;
	s_gs_sw_threads[index].handle = std::move(thread);
}

u64 PerformanceMetrics::GetFrameNumber()
{
	return s_frame_number;
}

PerformanceMetrics::InternalFPSMethod PerformanceMetrics::GetInternalFPSMethod()
{
	return s_internal_fps_method;
}

bool PerformanceMetrics::IsInternalFPSValid()
{
	return s_internal_fps_method != InternalFPSMethod::None;
}

float PerformanceMetrics::GetFPS()
{
	return s_fps;
}

float PerformanceMetrics::GetInternalFPS()
{
	return s_internal_fps;
}

float PerformanceMetrics::GetSpeed()
{
	return (s_fps / VMManager::GetFrameRate()) * 100.0;
}

float PerformanceMetrics::GetAverageFrameTime()
{
	return s_average_frame_time;
}

float PerformanceMetrics::GetMinimumFrameTime()
{
	return s_minimum_frame_time;
}

float PerformanceMetrics::GetMaximumFrameTime()
{
	return s_maximum_frame_time;
}

double PerformanceMetrics::GetCPUThreadUsage()
{
	return s_cpu_thread_usage;
}

double PerformanceMetrics::GetCPUThreadAverageTime()
{
	return s_cpu_thread_time;
}

float PerformanceMetrics::GetGSThreadUsage()
{
	return s_gs_thread_usage;
}

float PerformanceMetrics::GetGSThreadAverageTime()
{
	return s_gs_thread_time;
}

float PerformanceMetrics::GetVUThreadUsage()
{
	return s_vu_thread_usage;
}

float PerformanceMetrics::GetVUThreadAverageTime()
{
	return s_vu_thread_time;
}

float PerformanceMetrics::GetCaptureThreadUsage()
{
	return s_capture_thread_usage;
}

float PerformanceMetrics::GetCaptureThreadAverageTime()
{
	return s_capture_thread_time;
}

u32 PerformanceMetrics::GetGSSWThreadCount()
{
	return static_cast<u32>(s_gs_sw_threads.size());
}

double PerformanceMetrics::GetGSSWThreadUsage(u32 index)
{
	return s_gs_sw_threads[index].usage;
}

double PerformanceMetrics::GetGSSWThreadAverageTime(u32 index)
{
	return s_gs_sw_threads[index].time;
}

float PerformanceMetrics::GetGPUUsage()
{
	return s_gpu_usage;
}

float PerformanceMetrics::GetGPUAverageTime()
{
	return s_average_gpu_time;
}

const PerformanceMetrics::FrameTimeHistory& PerformanceMetrics::GetFrameTimeHistory()
{
	return s_frame_time_history;
}

u32 PerformanceMetrics::GetFrameTimeHistoryPos()
{
	return s_frame_time_history_pos;
}
