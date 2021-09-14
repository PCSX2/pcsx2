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

#include "Timer.h"
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#include "RedtapeWindows.h"
#elif defined(__APPLE__)
#include <mach/mach_init.h>
#include <mach/thread_act.h>
#include <mach/mach_port.h>
#else
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#endif

namespace Common
{

#ifdef _WIN32

	static double s_counter_frequency;
	static bool s_counter_initialized = false;

	Timer::Value Timer::GetCurrentValue()
	{
		// even if this races, it should still result in the same value..
		if (!s_counter_initialized)
		{
			LARGE_INTEGER Freq;
			QueryPerformanceFrequency(&Freq);
			s_counter_frequency = static_cast<double>(Freq.QuadPart) / 1000000000.0;
			s_counter_initialized = true;
		}

		Timer::Value ReturnValue;
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&ReturnValue));
		return ReturnValue;
	}

	double Timer::ConvertValueToNanoseconds(Timer::Value value)
	{
		return (static_cast<double>(value) / s_counter_frequency);
	}

	double Timer::ConvertValueToMilliseconds(Timer::Value value)
	{
		return ((static_cast<double>(value) / s_counter_frequency) / 1000000.0);
	}

	double Timer::ConvertValueToSeconds(Timer::Value value)
	{
		return ((static_cast<double>(value) / s_counter_frequency) / 1000000000.0);
	}

	Timer::Value Timer::ConvertSecondsToValue(double s)
	{
		return static_cast<Value>((s * 1000000000.0) * s_counter_frequency);
	}

	Timer::Value Timer::ConvertMillisecondsToValue(double ms)
	{
		return static_cast<Value>((ms * 1000000.0) * s_counter_frequency);
	}

	Timer::Value Timer::ConvertNanosecondsToValue(double ns)
	{
		return static_cast<Value>(ns * s_counter_frequency);
	}

#else

	Timer::Value Timer::GetCurrentValue()
	{
		struct timespec tv;
		clock_gettime(CLOCK_MONOTONIC, &tv);
		return ((Value)tv.tv_nsec + (Value)tv.tv_sec * 1000000000);
	}

	double Timer::ConvertValueToNanoseconds(Timer::Value value)
	{
		return static_cast<double>(value);
	}

	double Timer::ConvertValueToMilliseconds(Timer::Value value)
	{
		return (static_cast<double>(value) / 1000000.0);
	}

	double Timer::ConvertValueToSeconds(Timer::Value value)
	{
		return (static_cast<double>(value) / 1000000000.0);
	}

	Timer::Value Timer::ConvertSecondsToValue(double s)
	{
		return static_cast<Value>(s * 1000000000.0);
	}

	Timer::Value Timer::ConvertMillisecondsToValue(double ms)
	{
		return static_cast<Value>(ms * 1000000.0);
	}

	Timer::Value Timer::ConvertNanosecondsToValue(double ns)
	{
		return static_cast<Value>(ns);
	}

#endif

	Timer::Timer()
	{
		Reset();
	}

	void Timer::Reset()
	{
		m_tvStartValue = GetCurrentValue();
	}

	double Timer::GetTimeSeconds() const
	{
		return ConvertValueToSeconds(GetCurrentValue() - m_tvStartValue);
	}

	double Timer::GetTimeMilliseconds() const
	{
		return ConvertValueToMilliseconds(GetCurrentValue() - m_tvStartValue);
	}

	double Timer::GetTimeNanoseconds() const
	{
		return ConvertValueToNanoseconds(GetCurrentValue() - m_tvStartValue);
	}

	double Timer::GetTimeSecondsAndReset()
	{
		const Value value = GetCurrentValue();
		const double ret = ConvertValueToSeconds(value - m_tvStartValue);
		m_tvStartValue = value;
		return ret;
	}

	double Timer::GetTimeMillisecondsAndReset()
	{
		const Value value = GetCurrentValue();
		const double ret = ConvertValueToMilliseconds(value - m_tvStartValue);
		m_tvStartValue = value;
		return ret;
	}

	double Timer::GetTimeNanosecondsAndReset()
	{
		const Value value = GetCurrentValue();
		const double ret = ConvertValueToNanoseconds(value - m_tvStartValue);
		m_tvStartValue = value;
		return ret;
	}

	ThreadCPUTimer::ThreadCPUTimer() = default;

	ThreadCPUTimer::ThreadCPUTimer(ThreadCPUTimer&& move)
		: m_thread_handle(move.m_thread_handle)
	{
		move.m_thread_handle = nullptr;
	}

	ThreadCPUTimer::~ThreadCPUTimer()
	{
#ifdef _WIN32
		CloseHandle(reinterpret_cast<HANDLE>(m_thread_handle));
#endif
	}

	ThreadCPUTimer& ThreadCPUTimer::operator=(ThreadCPUTimer&& move)
	{
		std::swap(m_thread_handle, move.m_thread_handle);
		return *this;
	}

	void ThreadCPUTimer::Reset()
	{
		m_start_value = GetCurrentValue();
	}

	ThreadCPUTimer::Value ThreadCPUTimer::GetCurrentValue() const
	{
#if defined(_WIN32)
		FILETIME create, exit, user, kernel;
		if (!m_thread_handle || !GetThreadTimes((HANDLE)m_thread_handle, &create, &exit, &user, &kernel))
			return 0;

		Value value = (static_cast<Value>(user.dwHighDateTime) << 32) | (static_cast<Value>(user.dwLowDateTime));
		value += (static_cast<Value>(kernel.dwHighDateTime) << 32) | (static_cast<Value>(kernel.dwLowDateTime));
		return value;
#elif defined(__APPLE__)
		thread_basic_info_data_t info;
		mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;

		const kern_return_t kr = thread_info((mach_port_t) reinterpret_cast<uintptr_t>(m_thread_handle), THREAD_BASIC_INFO, (thread_info_t)&info, &count);
		if (kr != KERN_SUCCESS)
			return 0;

		Value value = (static_cast<Value>(info.user_time.seconds) * 1000000) + (static_cast<Value>(info.user_time.microseconds));
		value += (static_cast<Value>(info.system_time.seconds) * 1000000) + (static_cast<Value>(info.system_time.microseconds));
		return value;
#else
		clockid_t cid;
		if (!m_thread_handle || pthread_getcpuclockid((pthread_t)m_thread_handle, &cid) != 0)
			return 0;

		struct timespec ts;
		if (clock_gettime(cid, &ts) != 0)
			return 0;

		return (static_cast<Value>(ts.tv_nsec) + static_cast<Value>(ts.tv_sec) * 1000000000LL);
#endif
	}

	double ThreadCPUTimer::GetTimeSeconds() const
	{
		return ConvertValueToSeconds(GetCurrentValue() - m_start_value);
	}

	double ThreadCPUTimer::GetTimeMilliseconds() const
	{
		return ConvertValueToMilliseconds(GetCurrentValue() - m_start_value);
	}

	double ThreadCPUTimer::GetTimeNanoseconds() const
	{
		return ConvertValueToNanoseconds(GetCurrentValue() - m_start_value);
	}

	void ThreadCPUTimer::GetUsageInSecondsAndReset(Value time_diff, double* usage_time, double* usage_percent)
	{
		const Value new_value = GetCurrentValue();
		const Value diff = new_value - m_start_value;
		m_start_value = new_value;

		*usage_time = ConvertValueToSeconds(diff);
		*usage_percent = GetUtilizationPercentage(time_diff, diff);
	}

	void ThreadCPUTimer::GetUsageInMillisecondsAndReset(Value time_diff, double* usage_time, double* usage_percent)
	{
		const Value new_value = GetCurrentValue();
		const Value diff = new_value - m_start_value;
		m_start_value = new_value;

		*usage_time = ConvertValueToMilliseconds(diff);
		*usage_percent = GetUtilizationPercentage(time_diff, diff);
	}

	void ThreadCPUTimer::GetUsageInNanosecondsAndReset(Value time_diff, double* usage_time, double* usage_percent)
	{
		const Value new_value = GetCurrentValue();
		const Value diff = new_value - m_start_value;
		m_start_value = new_value;

		*usage_time = ConvertValueToNanoseconds(diff);
		*usage_percent = GetUtilizationPercentage(time_diff, diff);
	}

	double ThreadCPUTimer::GetUtilizationPercentage(Timer::Value time_diff, Value cpu_time_diff)
	{
#if defined(_WIN32)
		return ((static_cast<double>(cpu_time_diff) * 10000.0) / (static_cast<double>(time_diff) / s_counter_frequency));
#elif defined(__APPLE__)
		// microseconds, but time_tiff is in nanoseconds, so multiply by 1000 * 100
		return (static_cast<double>(cpu_time_diff) * 100000.0) / static_cast<double>(time_diff);
#else
		// nanoseconds
		return (static_cast<double>(cpu_time_diff) * 100.0) / static_cast<double>(time_diff);
#endif
	}

	double ThreadCPUTimer::ConvertValueToSeconds(Value value)
	{
#if defined(_WIN32)
		// 100ns units
		return (static_cast<double>(value) / 10000000.0);
#elif defined(__APPLE__)
		// microseconds
		return (static_cast<double>(value) / 1000000.0);
#else
		// nanoseconds
		return (static_cast<double>(value) / 1000000000.0);
#endif
	}

	double ThreadCPUTimer::ConvertValueToMilliseconds(Value value)
	{
#if defined(_WIN32)
		return (static_cast<double>(value) / 10000.0);
#elif defined(__APPLE__)
		return (static_cast<double>(value) / 1000.0);
#else
		return (static_cast<double>(value) / 1000000.0);
#endif
	}

	double ThreadCPUTimer::ConvertValueToNanoseconds(Value value)
	{
#if defined(_WIN32)
		return (static_cast<double>(value) * 100.0);
#elif defined(__APPLE__)
		return (static_cast<double>(value) * 1000.0);
#else
		return static_cast<double>(value);
#endif
	}

	ThreadCPUTimer ThreadCPUTimer::GetForCallingThread()
	{
		ThreadCPUTimer ret;
#if defined(_WIN32)
		ret.m_thread_handle = (void*)OpenThread(THREAD_QUERY_INFORMATION, FALSE, GetCurrentThreadId());
#elif defined(__APPLE__)
		ret.m_thread_handle = reinterpret_cast<void*>((uintptr_t)mach_thread_self());
#else
		ret.m_thread_handle = (void*)pthread_self();
#endif
		ret.Reset();
		return ret;
	}

} // namespace Common
