/*  Cpudetection lib
 *  Copyright (C) 2002-2016  PCSX2 Dev Team
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

#if defined(_WIN32)

#include "common/Console.h"
#include "common/emitter/cpudetect_internal.h"

void x86capabilities::CountLogicalCores()
{
	DWORD_PTR vProcessCPUs;
	DWORD_PTR vSystemCPUs;

	LogicalCores = 1;

	if (!GetProcessAffinityMask(GetCurrentProcess(), &vProcessCPUs, &vSystemCPUs))
		return;

	uint CPUs = 0;
	for (DWORD_PTR bit = 1; bit != 0; bit <<= 1)
		if (vSystemCPUs & bit)
			CPUs++;

	LogicalCores = CPUs;
}

SingleCoreAffinity::SingleCoreAffinity()
{
	s_threadId = nullptr;
	s_oldmask = ERROR_INVALID_PARAMETER;

	DWORD_PTR availProcCpus;
	DWORD_PTR availSysCpus;
	if (!GetProcessAffinityMask(GetCurrentProcess(), &availProcCpus, &availSysCpus))
		return;

	int cpu = 0;
	DWORD_PTR affinityMask;
	for (affinityMask = 1; affinityMask != 0; affinityMask <<= 1, ++cpu)
		if (availProcCpus & affinityMask)
			break;

	s_threadId = GetCurrentThread();
	s_oldmask = SetThreadAffinityMask(s_threadId, affinityMask);

	if (s_oldmask == ERROR_INVALID_PARAMETER)
	{
		const int hexWidth = 2 * sizeof(DWORD_PTR);
		Console.Warning(
			"CpuDetect: SetThreadAffinityMask failed...\n"
			"\tSystem Affinity : 0x%0*x\n"
			"\tProcess Affinity: 0x%0*x\n"
			"\tAttempted Thread Affinity CPU: %i",
			hexWidth, availProcCpus, hexWidth, availSysCpus, cpu);
	}

	Sleep(2);

	// Sleep Explained: I arbitrarily pick Core 0 to lock to for running the CPU test.  This
	// means that the current thread will need to be switched to Core 0 if it's currently
	// scheduled on a difference cpu/core.  However, Windows does not necessarily perform
	// that scheduling immediately upon the call to SetThreadAffinityMask (seems dependent
	// on version: XP does, Win7 does not).  So by issuing a Sleep here we give Win7 time
	// to issue a timeslice and move our thread to Core 0.  Without this, it tends to move
	// the thread during the cpuSpeed test instead, causing totally wacky results.
};

SingleCoreAffinity::~SingleCoreAffinity()
{
	if (s_oldmask != ERROR_INVALID_PARAMETER)
		SetThreadAffinityMask(s_threadId, s_oldmask);
}
#endif
