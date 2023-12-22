// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#ifdef __APPLE__

#include <string>
#include <vector>

#include "common/Pcsx2Types.h"

namespace DarwinMisc {

struct CPUClass {
	std::string name;
	u32 num_physical;
	u32 num_logical;
};

std::vector<CPUClass> GetCPUClasses();

}

#endif
