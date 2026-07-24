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

	static constexpr u32 NUM_FRAME_TIME_SAMPLES = 150;
	using FrameTimeHistory = std::array<float, NUM_FRAME_TIME_SAMPLES>;

	void Clear();
	void Reset();
	void Update(bool gs_register_write, bool fb_blit, bool is_skipping_present);
	void OnGPUPresent(float gpu_time, u64 vs_invocations, u64 ps_invocations);

	/// Logs the whole-session average framerate (frames since Clear() over
	/// wall time). Called at VM shutdown so every -logfile run records it.
	void LogSessionSummary();

	/// Android ADPF (PerformanceHintManager): register the calling thread as perf-critical
	/// so the OS ramps its CPU/GPU clocks toward the frame deadline instead of leaving them
	/// low under emulation's bursty load. Called on the EE (CPU), GS and MTVU threads.
	/// No-op on non-Android and below API 33 (symbols resolved via dlsym).
	void AdpfRegisterCallingThread();
	/// Enable/disable ADPF hinting at runtime (settings toggle). Default OFF (experimental).
	void AdpfSetEnabled(bool enabled);
	/// Close the ADPF session and forget registered threads (VM shutdown).
	void AdpfShutdown();

	/// ADPF work-period brackets, driven by the frame limiter (VMManager::Internal::Throttle).
	/// The reported duration must be the active EE/GS/VU work per frame, EXCLUDING the deliberate
	/// limiter sleep and present wait, per the PerformanceHintManager contract (reportActualWork
	/// = the last workload cycle, not the frame interval). OnFrameWorkComplete() reports the
	/// period that just ended (called at Throttle entry, before the sleep); BeginFrameWork()
	/// opens a new period after the sleep; PauseFrameWork() invalidates it when we are not
	/// frame-limiting (unlimited / host-vsync pacing), so no bogus duration is reported.
	void AdpfOnFrameWorkComplete();
	void AdpfBeginFrameWork();
	void AdpfPauseFrameWork();

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
	/// GPU time for the most recent present only, not a window average. Used by the
	/// per-frame stats series, where an averaged value would hide the spike.
	float GetLastGPUTime();
	double GetGPUAverageVSInvocations();
	double GetGPUAveragePSInvocations();

	const FrameTimeHistory& GetFrameTimeHistory();
	u32 GetFrameTimeHistoryPos();
} // namespace PerformanceMetrics
