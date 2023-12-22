// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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

		bool ResetIfSecondsPassed(double s);
		bool ResetIfMillisecondsPassed(double s);
		bool ResetIfNanosecondsPassed(double s);

	private:
		Value m_tvStartValue;
	};
} // namespace Common