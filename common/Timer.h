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

#pragma once
#include <cstdint>

namespace Common
{

	class Timer
	{
	public:
		using Value = std::uint64_t;

		Timer();

		static Value GetCurrentValue();
		static double ConvertValueToSeconds(Value value);
		static double ConvertValueToMilliseconds(Value value);
		static double ConvertValueToNanoseconds(Value value);
		static Value ConvertSecondsToValue(double s);
		static Value ConvertMillisecondsToValue(double s);
		static Value ConvertNanosecondsToValue(double ns);

		void Reset();
		void ResetTo(Value value) { m_tvStartValue = value; }

		Value GetStartValue() const { return m_tvStartValue; }

		double GetTimeSeconds() const;
		double GetTimeMilliseconds() const;
		double GetTimeNanoseconds() const;

		double GetTimeSecondsAndReset();
		double GetTimeMillisecondsAndReset();
		double GetTimeNanosecondsAndReset();

	private:
		Value m_tvStartValue;
	};

	class ThreadCPUTimer
	{
	public:
		using Value = std::uint64_t;

		ThreadCPUTimer();
		ThreadCPUTimer(ThreadCPUTimer&& move);
		ThreadCPUTimer(const ThreadCPUTimer&) = delete;
		~ThreadCPUTimer();

		void Reset();
		void ResetTo(Value value) { m_start_value = value; }

		Value GetStartValue() const { return m_start_value; }
		Value GetCurrentValue() const;

		double GetTimeSeconds() const;
		double GetTimeMilliseconds() const;
		double GetTimeNanoseconds() const;

		void GetUsageInSecondsAndReset(Value time_diff, double* usage_time, double* usage_percent);
		void GetUsageInMillisecondsAndReset(Value time_diff, double* usage_time, double* usage_percent);
		void GetUsageInNanosecondsAndReset(Value time_diff, double* usage_time, double* usage_percent);

		static double GetUtilizationPercentage(Timer::Value time_diff, Value cpu_time_diff);

		static double ConvertValueToSeconds(Value value);
		static double ConvertValueToMilliseconds(Value value);
		static double ConvertValueToNanoseconds(Value value);

		static ThreadCPUTimer GetForCallingThread();

		ThreadCPUTimer& operator=(const ThreadCPUTimer&) = delete;
		ThreadCPUTimer& operator=(ThreadCPUTimer&& move);

	private:
		void* m_thread_handle = nullptr;
		Value m_start_value = 0;
	};

} // namespace Common