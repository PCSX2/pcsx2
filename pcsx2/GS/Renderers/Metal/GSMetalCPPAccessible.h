// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <string>
#include <vector>

// Header with all metal stuff available for use with C++ (rather than Objective-C++)

#ifdef __APPLE__

class GSDevice;
GSDevice* MakeGSDeviceMTL();
std::vector<std::string> GetMetalAdapterList();

#endif
