// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Timer.h"
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#include "RedtapeWindows.h"
#else
#include <time.h>
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


	bool Timer::ResetIfSecondsPassed(double s)
	{
		const Value value = GetCurrentValue();
		const double ret = ConvertValueToSeconds(value - m_tvStartValue);
		if (ret < s)
			return false;

		m_tvStartValue = value;
		return true;
	}

	bool Timer::ResetIfMillisecondsPassed(double s)
	{
		const Value value = GetCurrentValue();
		const double ret = ConvertValueToMilliseconds(value - m_tvStartValue);
		if (ret < s)
			return false;

		m_tvStartValue = value;
		return true;
	}

	bool Timer::ResetIfNanosecondsPassed(double s)
	{
		const Value value = GetCurrentValue();
		const double ret = ConvertValueToNanoseconds(value - m_tvStartValue);
		if (ret < s)
			return false;

		m_tvStartValue = value;
		return true;
	}
} // namespace Common
