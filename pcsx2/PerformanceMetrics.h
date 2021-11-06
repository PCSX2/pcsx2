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
#include "common/Timer.h"

namespace PerformanceMetrics
{
	void Clear();
	void Reset();
	void Update();

	/// Sets the EE thread for CPU usage calculations.
	void SetCPUThreadTimer(Common::ThreadCPUTimer timer);

	/// Sets the vertical frequency, used in speed calculations.
	void SetVerticalFrequency(float rate);

	float GetFPS();
	float GetSpeed();
	float GetAverageFrameTime();
	float GetWorstFrameTime();

	double GetCPUThreadUsage();
	double GetCPUThreadAverageTime();
	float GetGSThreadUsage();
	float GetGSThreadAverageTime();
	float GetVUThreadUsage();
	float GetVUThreadAverageTime();
} // namespace PerformanceMetrics