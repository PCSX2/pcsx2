// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#if defined(_WIN32)

#include "common/Console.h"
#include "common/emitter/tools.h"
#include "common/RedtapeWindows.h"

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

#endif
