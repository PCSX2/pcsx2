// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/GS.h"

#include <string>
#include <vector>

// Header with all metal stuff available for use with C++ (rather than Objective-C++)

#ifdef __APPLE__

class GSDevice;
GSDevice* MakeGSDeviceMTL();
std::vector<GSAdapterInfo> GetMetalAdapterList();

#endif
