// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#ifdef _WIN32
#include "common/RedtapeWindows.h"

//This ensures that the nVidia graphics card is used for PCSX2 on an Optimus-enabled system.
//302 or higher driver required.
extern "C" {
	_declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}

//This is the equivalent for an AMD system.
//13.35 or newer driver required.
extern "C" {
	_declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

#endif
