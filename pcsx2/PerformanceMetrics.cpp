/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include <chrono>
#include <vector>

#include "PerformanceMetrics.h"
#include "System.h"
#include "System/SysThreads.h"

#include "GS.h"
#include "MTVU.h"

#ifdef PCSX2_CORE
#include "VMManager.h"
#endif

static const float UPDATE_INTERVAL = 0.5f;

static float s_vertical_frequency = 0.0f;
static float s_fps = 0.0f;
static float s_internal_fps = 0.0f;
static float s_worst_frame_time = 0.0f;
static float s_average_frame_time = 0.0f;
static float s_average_frame_time_accumulator = 0.0f;
static float s_worst_frame_time_accumulator = 0.0f;
static u32 s_frames_since_last_update = 0;
static Common::Timer s_last_update_time;
static Common::Timer s_last_frame_time;

// frame number, updated by the GS thread
static u64 s_frame_number = 0;

// internal fps heuristics
static PerformanceMetrics::InternalFPSMethod s_internal_fps_method = PerformanceMetrics::InternalFPSMethod::None;
static u32 s_gs_framebuffer_blits_since_last_update = 0;
static u32 s_gs_privileged_register_writes_since_last_update = 0;

static Common::ThreadCPUTimer s_cpu_thread_timer;
static u64 s_last_gs_time = 0;
static u64 s_last_vu_time = 0;
static u64 s_last_ticks = 0;

static double s_cpu_thread_usage = 0.0f;
static double s_cpu_thread_time = 0.0f;
static float s_gs_thread_usage = 0.0f;
static float s_gs_thread_time = 0.0f;
static float s_vu_thread_usage = 0.0f;
static float s_vu_thread_time = 0.0f;

struct GSSWThreadStats
{
	Common::ThreadCPUTimer timer;
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
	s_worst_frame_time = 0.0f;
	s_average_frame_time = 0.0f;
	s_internal_fps_method = PerformanceMetrics::InternalFPSMethod::None;

	s_cpu_thread_usage = 0.0f;
	s_cpu_thread_time = 0.0f;
	s_gs_thread_usage = 0.0f;
	s_gs_thread_time = 0.0f;
	s_vu_thread_usage = 0.0f;
	s_vu_thread_time = 0.0f;

	s_average_gpu_time = 0.0f;
	s_gpu_usage = 0.0f;

	s_frame_number = 0;
}

void PerformanceMetrics::Reset()
{
	s_frames_since_last_update = 0;
	s_gs_framebuffer_blits_since_last_update = 0;
	s_gs_privileged_register_writes_since_last_update = 0;
	s_average_frame_time_accumulator = 0.0f;
	s_worst_frame_time_accumulator = 0.0f;

	s_accumulated_gpu_time = 0.0f;
	s_presents_since_last_update = 0;

	s_last_update_time.Reset();
	s_last_frame_time.Reset();

	s_cpu_thread_timer.Reset();
	s_last_gs_time = GetMTGS().GetCpuTime();
	s_last_vu_time = THREAD_VU1 ? vu1Thread.GetCpuTime() : 0;
	s_last_ticks = GetCPUTicks();
}

void PerformanceMetrics::Update(bool gs_register_write, bool fb_blit)
{
	const float frame_time = s_last_frame_time.GetTimeMillisecondsAndReset();
	s_average_frame_time_accumulator += frame_time;
	s_worst_frame_time_accumulator = std::max(s_worst_frame_time_accumulator, frame_time);
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
	s_worst_frame_time = s_worst_frame_time_accumulator;
	s_worst_frame_time_accumulator = 0.0f;
	s_average_frame_time = s_average_frame_time_accumulator / static_cast<float>(s_frames_since_last_update);
	s_average_frame_time_accumulator = 0.0f;
	s_fps = static_cast<float>(s_frames_since_last_update) / time;
	s_average_gpu_time = s_accumulated_gpu_time / static_cast<float>(s_frames_since_last_update);
	s_gpu_usage = s_accumulated_gpu_time / (time * 10.0f);
	s_accumulated_gpu_time = 0.0f;

	// prefer privileged register write based framerate detection, it's less likely to have false positives
	if (s_gs_privileged_register_writes_since_last_update > 0)
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

	s_cpu_thread_timer.GetUsageInMillisecondsAndReset(ticks_diff, &s_cpu_thread_time, &s_cpu_thread_usage);
	s_cpu_thread_time /= static_cast<double>(s_frames_since_last_update);

	for (GSSWThreadStats& thread : s_gs_sw_threads)
	{
		thread.timer.GetUsageInMillisecondsAndReset(ticks_diff, &thread.time, &thread.usage);
		thread.time /= static_cast<double>(s_frames_since_last_update);
	}

	const u64 gs_time = GetMTGS().GetCpuTime();
	const u64 vu_time = THREAD_VU1 ? vu1Thread.GetCpuTime() : 0;
	const u64 ticks = GetCPUTicks();

	const u64 gs_delta = gs_time - s_last_gs_time;
	const u64 vu_delta = vu_time - s_last_vu_time;
	const u64 ticks_delta = ticks - s_last_ticks;

	const double pct_divider =
		100.0 * (1.0 / ((static_cast<double>(ticks_delta) * static_cast<double>(GetThreadTicksPerSecond())) /
						   static_cast<double>(GetTickFrequency())));
	const double time_divider = 1000.0 * (1.0 / static_cast<double>(GetThreadTicksPerSecond())) *
								(1.0 / static_cast<double>(s_frames_since_last_update));

	s_gs_thread_usage = static_cast<double>(gs_delta) * pct_divider;
	s_vu_thread_usage = static_cast<double>(vu_delta) * pct_divider;
	s_gs_thread_time = static_cast<double>(gs_delta) * time_divider;
	s_vu_thread_time = static_cast<double>(vu_delta) * time_divider;

	s_last_gs_time = gs_time;
	s_last_vu_time = vu_time;
	s_last_ticks = ticks;

	s_frames_since_last_update = 0;
	s_presents_since_last_update = 0;

#ifdef PCSX2_CORE
	Host::OnPerformanceMetricsUpdated();
#endif
}

void PerformanceMetrics::OnGPUPresent(float gpu_time)
{
	s_accumulated_gpu_time += gpu_time;
	s_presents_since_last_update++;
}

void PerformanceMetrics::SetCPUThreadTimer(Common::ThreadCPUTimer timer)
{
	s_cpu_thread_timer = std::move(timer);
}

void PerformanceMetrics::SetGSSWThreadCount(u32 count)
{
	s_gs_sw_threads.clear();
	s_gs_sw_threads.resize(count);
}

void PerformanceMetrics::SetGSSWThreadTimer(u32 index, Common::ThreadCPUTimer timer)
{
	s_gs_sw_threads[index].timer = std::move(timer);
}

void PerformanceMetrics::SetVerticalFrequency(float rate)
{
	s_vertical_frequency = rate;
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
	return (s_fps / s_vertical_frequency) * 100.0;
}

float PerformanceMetrics::GetAverageFrameTime()
{
	return s_average_frame_time;
}

float PerformanceMetrics::GetWorstFrameTime()
{
	return s_worst_frame_time;
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
